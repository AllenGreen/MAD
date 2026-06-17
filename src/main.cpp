#include "core/coords_dump.hpp"
#include "core/engine.hpp"
#include "core/log.hpp"
#include "core/recorder.hpp"
#include "core/scenario.hpp"

#include <cstring>
#include <string>

namespace {

void print_usage() {
    mad::core::log::info("Main",
        "usage: mad [--record <scenario.mad> --out <dir>] [--windowed]");
}

} // namespace

int main(int argc, char* argv[]) {
    mad::core::log::info("Main", "Mages Against Demons v0.1.0");

    std::string scenario_path;
    std::string out_dir = "/Data/mad_capture/frames";
    bool record = false;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--record") == 0 && i + 1 < argc) {
            record = true;
            scenario_path = argv[++i];
        } else if (std::strcmp(a, "--out") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (std::strcmp(a, "--coords") == 0 && i + 1 < argc) {
            return mad::core::dump_coords(argv[++i]) ? 0 : 1;
        } else if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    if (record) {
        mad::core::Scenario scenario;
        std::string err;
        if (!mad::core::load_scenario_file(scenario_path, scenario, err)) {
            mad::core::log::error("Main", "scenario error: {}", err);
            return 1;
        }
        mad::core::RecordResult res = mad::core::record_scenario(scenario, out_dir);
        if (!res.ok) {
            mad::core::log::error("Main", "record failed: {}", res.error);
            return 1;
        }
        mad::core::log::info("Main", "recorded {} frames; {} demons reached the nexus",
                             res.frames, res.demons_reached_nexus);
        return 0;
    }

    // Windowed mode (interactive).
    mad::core::EngineConfig config;
    config.window_title = "Mages Against Demons";
    config.window_width = 1280;
    config.window_height = 720;

    mad::core::Engine engine(config);
    engine.run();
    return 0;
}
