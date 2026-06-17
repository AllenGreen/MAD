#include "core/recorder.hpp"
#include "core/log.hpp"
#include "core/world.hpp"
#include "rendering/camera.hpp"
#include "rendering/renderer.hpp"

#include <SDL2/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace mad::core {

namespace {

constexpr const char* TAG = "Recorder";
namespace fs = std::filesystem;

rendering::Camera make_camera(const CameraSpec& spec, const game::GameMap& map) {
    const double R = map.config().map_radius;
    const int N = map.config().num_players;
    if (spec.kind == CameraSpec::Kind::Overview) {
        return rendering::Camera::fit({0, 0}, R * 1.08, 0.0, spec.width, spec.height);
    }
    const double rot = 2.0 * M_PI * spec.sector_id / N;
    const game::WorldPos center{std::sin(rot) * R * 0.5, std::cos(rot) * R * 0.5};
    return rendering::Camera::fit(center, R * 0.6, rot, spec.width, spec.height);
}

// Write an RGB24 buffer as a binary PPM (P6).
bool write_ppm(const fs::path& path, const std::vector<uint8_t>& rgb, int w, int h) {
    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    const size_t n = std::fwrite(rgb.data(), 1, rgb.size(), f);
    std::fclose(f);
    return n == rgb.size() && static_cast<size_t>(w) * h * 3 == rgb.size();
}

} // namespace

RecordResult record_scenario(const Scenario& scenario, const std::string& out_dir) {
    RecordResult result;

    // Keep capture logs quiet unless explicitly debugging.
    if (!std::getenv("MAD_LOG_DEBUG"))
        log::set_level(log::Level::Info);

    // Headless: no display needed.
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "offscreen");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        // Fall back to the dummy driver if offscreen is unavailable.
        SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            result.error = std::string("SDL_Init failed: ") + SDL_GetError();
            return result;
        }
    }

    int max_w = 1, max_h = 1;
    for (const auto& c : scenario.cameras) {
        max_w = std::max(max_w, c.width);
        max_h = std::max(max_h, c.height);
    }

    SDL_Window* window = SDL_CreateWindow("mad-headless", 0, 0, max_w, max_h,
                                          SDL_WINDOW_HIDDEN);
    if (!window) {
        result.error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        SDL_Quit();
        return result;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        result.error = std::string("SDL_CreateRenderer failed: ") + SDL_GetError();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return result;
    }

    World world(scenario.map, scenario.seed);
    rendering::Renderer gfx;

    // One render-target texture and camera per camera spec.
    struct Cam {
        const CameraSpec* spec;
        rendering::Camera cam;
        SDL_Texture* tex;
        fs::path dir;
        std::vector<uint8_t> buf;
    };
    std::vector<Cam> cams;
    for (const auto& spec : scenario.cameras) {
        SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_TARGET, spec.width, spec.height);
        if (!tex) {
            result.error = std::string("SDL_CreateTexture failed: ") + SDL_GetError();
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return result;
        }
        fs::path dir = fs::path(out_dir) / spec.name;
        std::error_code ec;
        fs::create_directories(dir, ec);
        cams.push_back(Cam{&spec, make_camera(spec, world.map()), tex, dir,
                           std::vector<uint8_t>(static_cast<size_t>(spec.width) * spec.height * 3)});
    }

    const double dt = 1.0 / scenario.tick_rate;
    const int frame_interval = std::max(1, static_cast<int>(
        std::lround(scenario.tick_rate / scenario.capture_fps)));
    uint64_t frame_no = 0;

    // Optional path trails (MAD_TRAILS=1) drawn into the frames; MAD_TRAJ=<file>
    // records the same paths for plotting. Either one tracks positions.
    const bool use_trails = std::getenv("MAD_TRAILS") != nullptr;
    const bool want_traj = std::getenv("MAD_TRAJ") != nullptr;
    const bool track = use_trails || want_traj;
    const std::size_t trail_cap = want_traj ? 1000000 : 700; // full path for plots
    rendering::Trails trails;

    for (uint64_t tick = 0; tick < scenario.ticks; ++tick) {
        scenario.apply_events_at(world, tick);
        world.step(dt);

        if (tick % static_cast<uint64_t>(frame_interval) != 0) continue;
        ++frame_no;
        if (track) {
            for (const Demon& d : world.demons()) {
                auto& t = trails[d.id];
                t.push_back(d.pos);
                if (t.size() > trail_cap) t.erase(t.begin());
            }
        }
        for (Cam& c : cams) {
            SDL_SetRenderTarget(renderer, c.tex);
            gfx.draw(renderer, c.cam, world, use_trails ? &trails : nullptr);
            SDL_Rect rect{0, 0, c.spec->width, c.spec->height};
            if (SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_RGB24,
                                     c.buf.data(), c.spec->width * 3) != 0) {
                result.error = std::string("RenderReadPixels failed: ") + SDL_GetError();
            }
            char fname[32];
            std::snprintf(fname, sizeof(fname), "frame_%06llu.ppm",
                          static_cast<unsigned long long>(frame_no));
            write_ppm(c.dir / fname, c.buf, c.spec->width, c.spec->height);
        }
    }

    // Optional trajectory dump (MAD_TRAJ=<file>): every demon's path, for plots.
    if (const char* tp = std::getenv("MAD_TRAJ")) {
        if (std::FILE* tf = std::fopen(tp, "wb")) {
            std::fprintf(tf, "id\tframe\tx\ty\tr\n");
            for (const auto& [id, pts] : trails)
                for (std::size_t i = 0; i < pts.size(); ++i)
                    std::fprintf(tf, "%u\t%zu\t%g\t%g\t%g\n", id, i, pts[i].x, pts[i].y,
                                 std::hypot(pts[i].x, pts[i].y));
            std::fclose(tf);
        }
    }

    // Manifest for the capture tool.
    {
        fs::path mpath = fs::path(out_dir) / "manifest.txt";
        std::FILE* f = std::fopen(mpath.string().c_str(), "wb");
        if (f) {
            std::fprintf(f, "name\t%s\n", scenario.name.c_str());
            std::fprintf(f, "fps\t%g\n", scenario.capture_fps);
            std::fprintf(f, "ticks\t%llu\n", static_cast<unsigned long long>(scenario.ticks));
            std::fprintf(f, "frames\t%llu\n", static_cast<unsigned long long>(frame_no));
            std::fprintf(f, "reached_nexus\t%d\n", world.demons_reached_nexus());
            for (const Cam& c : cams)
                std::fprintf(f, "camera\t%s\t%d\t%d\n", c.spec->name.c_str(),
                             c.spec->width, c.spec->height);
            std::fclose(f);
        }
    }

    for (Cam& c : cams) SDL_DestroyTexture(c.tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    log::info(TAG, "recorded {} frames x {} cameras to {}", frame_no, cams.size(), out_dir);
    result.ok = true;
    result.frames = frame_no;
    result.demons_reached_nexus = world.demons_reached_nexus();
    return result;
}

} // namespace mad::core
