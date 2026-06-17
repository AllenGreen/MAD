#pragma once

#include "core/world.hpp"
#include "game/game_map.hpp"
#include "game/grid_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace mad::core {

// A scenario is a fully declarative description of a headless run: the map, the
// cameras to record, and a timeline of human-mimicking inputs (spawns, walls,
// towers). Authored as a small line-based text file so an AI agent can write
// runs without touching code. See documentation/ai_harness.md for the grammar.
struct CameraSpec {
    enum class Kind { Overview, Sector };
    Kind kind = Kind::Overview;
    int sector_id = 0;
    std::string name = "overview";
    int width = 720;
    int height = 540;
};

struct TimedEvent {
    enum class Type { Spawn, Wall, Tower, Maze, Border, Rung, Spiral };
    uint64_t tick = 0;
    Type type = Type::Spawn;

    bool spawn_nexus_goal = false;       // Spawn: head straight to the Nexus
    double spiral_pitch = 4.0;           // Spiral: radial distance per turn
    int spiral_dir = 1;                  // Spiral: +1 CW, -1 CCW

    // Maze: deterministic seed (derived from the scenario seed if not given).
    uint64_t maze_seed = 0;
    bool maze_has_seed = false;

    // Border ("along"): seal `count` seam crossings between `sector` and
    // `neighbor` from `border_index` outward.
    int neighbor = 1;
    int border_index = 0;

    // Rung ("perpendicular"): a tangential wall across `sector` at grid `cell.row`
    // with a gap of `count` cells at the `rung_gap_left` border.
    bool rung_gap_left = true;

    // Spawn
    int sector = 0;
    game::MoveType move_type = game::MoveType::Ground;
    int size = 1;
    int count = 1;
    int wave_dir = +1;

    // Wall / Tower share col/row/sector; Wall adds edge+height, Tower adds size.
    game::CellCoord cell{};
    game::EdgeType edge = game::EdgeType::Horizontal;
    int height = 2;
};

struct Scenario {
    std::string name = "untitled";
    game::MapConfig map{};
    uint64_t seed = 0x9A11DE05u;
    uint64_t ticks = 600;
    double tick_rate = 60.0;
    double capture_fps = 30.0;
    std::vector<CameraSpec> cameras;
    std::vector<TimedEvent> events; // sorted by tick after parse

    // Apply every event scheduled for exactly `tick` to the world.
    void apply_events_at(World& world, uint64_t tick) const;
};

// Parse a scenario from text. On failure returns false and fills `error`.
bool parse_scenario(const std::string& text, Scenario& out, std::string& error);
bool load_scenario_file(const std::string& path, Scenario& out, std::string& error);

} // namespace mad::core
