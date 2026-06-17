#include "core/scenario.hpp"
#include "core/log.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace mad::core {

namespace {

constexpr const char* TAG = "Scenario";

// Split a line into whitespace-separated tokens, dropping inline `#` comments.
std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) {
        if (!tok.empty() && tok[0] == '#') break;
        tokens.push_back(tok);
    }
    return tokens;
}

// Parse `key=value` tokens into a map (tokens without '=' are ignored here).
std::unordered_map<std::string, std::string> kv(const std::vector<std::string>& toks,
                                                size_t start) {
    std::unordered_map<std::string, std::string> m;
    for (size_t i = start; i < toks.size(); ++i) {
        const auto eq = toks[i].find('=');
        if (eq != std::string::npos)
            m[toks[i].substr(0, eq)] = toks[i].substr(eq + 1);
    }
    return m;
}

int get_int(const std::unordered_map<std::string, std::string>& m,
            const std::string& k, int def) {
    auto it = m.find(k);
    if (it == m.end()) return def;
    int v = def;
    std::from_chars(it->second.data(), it->second.data() + it->second.size(), v);
    return v;
}

double get_double(const std::unordered_map<std::string, std::string>& m,
                  const std::string& k, double def) {
    auto it = m.find(k);
    if (it == m.end()) return def;
    try {
        return std::stod(it->second);
    } catch (...) {
        return def;
    }
}

std::string get_str(const std::unordered_map<std::string, std::string>& m,
                    const std::string& k, const std::string& def) {
    auto it = m.find(k);
    return it == m.end() ? def : it->second;
}

game::MoveType parse_move_type(const std::string& s) {
    if (s == "climber") return game::MoveType::Climber;
    if (s == "flyer") return game::MoveType::Flyer;
    if (s == "smasher") return game::MoveType::Smasher;
    return game::MoveType::Ground;
}

game::EdgeType parse_edge_type(const std::string& s) {
    if (s == "vertical") return game::EdgeType::Vertical;
    if (s == "diagne") return game::EdgeType::DiagNE;
    if (s == "diagnw") return game::EdgeType::DiagNW;
    return game::EdgeType::Horizontal;
}

int parse_dir(const std::string& s) { return s == "ccw" ? -1 : +1; }

} // namespace

bool parse_scenario(const std::string& text, Scenario& out, std::string& error) {
    std::istringstream stream(text);
    std::string line;
    int lineno = 0;
    while (std::getline(stream, line)) {
        ++lineno;
        const auto toks = tokenize(line);
        if (toks.empty()) continue;
        const std::string& cmd = toks[0];

        if (cmd == "name") {
            const auto pos = line.find("name");
            out.name = pos == std::string::npos ? line
                                                : line.substr(pos + 4);
            // trim
            out.name.erase(0, out.name.find_first_not_of(" \t"));
            out.name.erase(out.name.find_last_not_of(" \t\r\n") + 1);
        } else if (cmd == "map") {
            auto m = kv(toks, 1);
            out.map.num_players = get_int(m, "players", out.map.num_players);
            out.map.grid_width = get_int(m, "grid_w", out.map.grid_width);
            out.map.grid_height = get_int(m, "grid_h", out.map.grid_height);
            out.map.map_radius = get_double(m, "radius", out.map.map_radius);
            out.map.cell_size = get_double(m, "cell", out.map.cell_size);
        } else if (cmd == "seed") {
            if (toks.size() >= 2) out.seed = std::stoull(toks[1]);
        } else if (cmd == "ticks") {
            if (toks.size() >= 2) out.ticks = std::stoull(toks[1]);
        } else if (cmd == "tick_rate") {
            if (toks.size() >= 2) out.tick_rate = std::stod(toks[1]);
        } else if (cmd == "fps") {
            if (toks.size() >= 2) out.capture_fps = std::stod(toks[1]);
        } else if (cmd == "camera") {
            if (toks.size() < 2) { error = "line " + std::to_string(lineno) + ": camera needs a kind"; return false; }
            auto m = kv(toks, 2);
            CameraSpec spec;
            if (toks[1] == "sector") {
                spec.kind = CameraSpec::Kind::Sector;
                spec.sector_id = get_int(m, "id", 0);
                spec.name = get_str(m, "name", "sector" + std::to_string(spec.sector_id));
                spec.width = get_int(m, "w", 640);
                spec.height = get_int(m, "h", 540);
            } else {
                spec.kind = CameraSpec::Kind::Overview;
                spec.name = get_str(m, "name", "overview");
                spec.width = get_int(m, "w", 720);
                spec.height = get_int(m, "h", 720);
            }
            out.cameras.push_back(spec);
        } else if (cmd == "at") {
            if (toks.size() < 3) { error = "line " + std::to_string(lineno) + ": 'at' needs tick and action"; return false; }
            const uint64_t tick = std::stoull(toks[1]);
            const std::string& action = toks[2];
            auto m = kv(toks, 3);
            TimedEvent ev;
            ev.tick = tick;
            ev.sector = get_int(m, "sector", 0);
            ev.cell = {get_int(m, "col", 0), get_int(m, "row", 0)};
            if (action == "spawn") {
                ev.type = TimedEvent::Type::Spawn;
                ev.move_type = parse_move_type(get_str(m, "type", "ground"));
                ev.size = get_int(m, "size", 1);
                ev.count = get_int(m, "count", 1);
                ev.wave_dir = parse_dir(get_str(m, "dir", "cw"));
                ev.spawn_nexus_goal = get_str(m, "goal", "shards") == "nexus";
            } else if (action == "spiral") {
                ev.type = TimedEvent::Type::Spiral;
                ev.spiral_pitch = get_double(m, "pitch", 4.0);
                ev.spiral_dir = parse_dir(get_str(m, "dir", "cw"));
                ev.height = get_int(m, "height", 2);
            } else if (action == "wall") {
                ev.type = TimedEvent::Type::Wall;
                ev.edge = parse_edge_type(get_str(m, "edge", "horizontal"));
                ev.height = get_int(m, "height", 2);
            } else if (action == "tower") {
                ev.type = TimedEvent::Type::Tower;
                ev.size = get_int(m, "size", 1);
            } else if (action == "maze") {
                ev.type = TimedEvent::Type::Maze;
                ev.height = get_int(m, "height", 2);
                if (m.count("seed")) {
                    ev.maze_seed = std::stoull(m.at("seed"));
                    ev.maze_has_seed = true;
                }
            } else if (action == "border") {
                // "along the boundary" seam seal at a row (radius)
                ev.type = TimedEvent::Type::Border;
                ev.neighbor = get_int(m, "neighbor", 1);
                ev.border_index = get_int(m, "row", get_int(m, "index", 0));
                ev.count = get_int(m, "count", 1);
                ev.height = get_int(m, "height", 2);
            } else if (action == "rung") {
                // "perpendicular" tangential rung across the wedge with a gap
                ev.type = TimedEvent::Type::Rung;
                ev.cell = {0, get_int(m, "row", 0)};
                ev.count = get_int(m, "gap", 3); // gap width in cells
                ev.height = get_int(m, "height", 2);
                ev.rung_gap_left = get_str(m, "side", "left") == "left";
            } else {
                error = "line " + std::to_string(lineno) + ": unknown action '" + action + "'";
                return false;
            }
            out.events.push_back(ev);
        } else {
            error = "line " + std::to_string(lineno) + ": unknown command '" + cmd + "'";
            return false;
        }
    }

    if (out.cameras.empty()) {
        CameraSpec spec; // default to a single overview camera
        out.cameras.push_back(spec);
    }
    std::stable_sort(out.events.begin(), out.events.end(),
                     [](const TimedEvent& a, const TimedEvent& b) { return a.tick < b.tick; });
    log::info(TAG, "parsed '{}': {} cameras, {} events, {} ticks",
              out.name, out.cameras.size(), out.events.size(), out.ticks);
    return true;
}

bool load_scenario_file(const std::string& path, Scenario& out, std::string& error) {
    std::ifstream f(path);
    if (!f) {
        error = "cannot open scenario file: " + path;
        return false;
    }
    std::stringstream buf;
    buf << f.rdbuf();
    return parse_scenario(buf.str(), out, error);
}

void Scenario::apply_events_at(World& world, uint64_t tick) const {
    for (const TimedEvent& ev : events) {
        if (ev.tick != tick) continue;
        switch (ev.type) {
            case TimedEvent::Type::Spawn:
                world.spawn_wave(ev.sector, ev.move_type, ev.size, ev.count,
                                 ev.wave_dir, ev.spawn_nexus_goal);
                break;
            case TimedEvent::Type::Spiral:
                world.add_perfect_spiral(ev.spiral_pitch, ev.spiral_dir,
                                         static_cast<uint8_t>(ev.height));
                break;
            case TimedEvent::Type::Wall: {
                game::EdgeCoord edge{ev.cell, ev.edge};
                world.map().sector(ev.sector).grid().walls().add(
                    edge, game::WallData{static_cast<uint8_t>(ev.height), 255});
                world.fields().invalidate_all();
                break;
            }
            case TimedEvent::Type::Tower:
                world.place_tower(ev.sector, ev.cell, ev.size);
                break;
            case TimedEvent::Type::Maze: {
                // Derive a stable per-sector seed from the scenario seed when the
                // event doesn't pin one explicitly.
                const uint64_t s = ev.maze_has_seed
                    ? ev.maze_seed
                    : seed ^ (0x9E3779B97F4A7C15ull * (ev.sector + 1));
                world.generate_maze(ev.sector, s, static_cast<uint8_t>(ev.height));
                break;
            }
            case TimedEvent::Type::Border: {
                for (int k = 0; k < std::max(1, ev.count); ++k)
                    world.add_boundary_wall(ev.sector, ev.neighbor,
                                            ev.border_index + k,
                                            static_cast<uint8_t>(ev.height));
                break;
            }
            case TimedEvent::Type::Rung:
                world.add_rung(ev.sector, ev.cell.row, ev.rung_gap_left, ev.count,
                               static_cast<uint8_t>(ev.height));
                break;
        }
    }
}

} // namespace mad::core
