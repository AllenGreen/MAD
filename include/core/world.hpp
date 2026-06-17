#pragma once

#include "game/flow_field_manager.hpp"
#include "game/game_map.hpp"
#include "game/grid_types.hpp"

#include <cstdint>
#include <random>
#include <span>
#include <unordered_map>
#include <vector>

namespace mad::core {

class World; // serialization hooks (see core/serialize.hpp)
std::vector<uint8_t> serialize_world(const World&);
bool deserialize_world(World&, std::span<const uint8_t>);

// A single demon marching the shard route: from its spawn portal it visits every
// crystal in rotational order (wave_dir), then heads to the Nexus. Steering is
// flow-field based; cross-sector hops use boundary fields.
struct Demon {
    uint32_t id = 0;
    game::WorldPos pos{};
    game::MoveType move_type = game::MoveType::Ground;
    int size = 1;
    double speed = 3.0;
    double hp = 10.0;
    double max_hp = 10.0;

    int spawn_sector = 0;
    int sector = 0;            // sector the demon currently occupies
    int wave_dir = +1;         // +1 = CW, -1 = CCW
    int shards_collected = 0;  // also the index of the crystal currently sought
    bool reached_nexus = false;
    bool alive = true;
};

// An "along the boundary" wall: a segment lying along the seam between two
// adjacent sectors, at a position (index) along the shared boundary line
// (boundary_cells(a,b), index 0 = nearest the nexus). It seals demons from
// *crossing* the seam there -- the exception wall that joins the two coordinate
// systems. (The other build option, a "perpendicular" rung across a wedge, is a
// plain set of grid walls; see World::add_rung.)
struct BoundaryWall {
    int a = 0;        // builder's sector
    int b = 0;        // neighbor sector across the seam
    int index = 0;    // position along boundary_cells(a,b), 0 = nearest nexus
    uint8_t height = 2;
};

// Owns the whole simulation: the map, the flow-field manager, and all demons.
// Pure, deterministic, fixed-timestep -- no rendering, no wall-clock. Stepping
// the same World with the same inputs always yields identical state.
class World {
public:
    explicit World(const game::MapConfig& config, uint64_t seed = 0x9A11DE05u);

    void step(double dt);
    uint64_t tick() const { return tick_; }

    game::GameMap& map() { return map_; }
    const game::GameMap& map() const { return map_; }
    game::FlowFieldManager& fields() { return fields_; }

    const std::vector<Demon>& demons() const { return demons_; }
    int live_demon_count() const;
    int demons_reached_nexus() const { return reached_nexus_count_; }

    // --- Human-mimicking inputs (also used by the scenario script) ---

    // Spawn `count` demons at `sector`'s portal edge. If `nexus_goal`, they skip
    // the shard route and head straight for the Nexus.
    void spawn_wave(int sector, game::MoveType mt, int size, int count, int wave_dir,
                    bool nexus_goal = false);
    // Place a wall edge in a sector's grid (invalidates flow fields).
    void place_wall(int sector, game::EdgeCoord edge);
    // Place a tower (size x size footprint) in a sector (invalidates flow fields).
    void place_tower(int sector, game::CellCoord origin, int size);
    // Fill a sector with a random "perfect" maze (recursive backtracker) over its
    // walkable cells, placing walls of `wall_height` on uncarved edges. Because a
    // perfect maze is a spanning tree, every walkable cell stays reachable, so the
    // crystal, nexus and boundary cells remain solvable. Deterministic in `seed`.
    void generate_maze(int sector, uint64_t seed, uint8_t wall_height = 2);

    // Seal the seam crossing between sectors `a` and `b` at position `index`
    // ("along the boundary" wall). Demons reroute to an open crossing.
    void add_boundary_wall(int a, int b, int index, uint8_t height = 2);
    const std::vector<BoundaryWall>& boundary_walls() const { return boundary_walls_; }
    // How many boundary positions exist between two adjacent sectors.
    int boundary_length(int a, int b);

    // Build a "perpendicular" rung across a wedge at grid row `row`: a tangential
    // wall on the edge between rows `row` and `row+1`, spanning the walkable width
    // except a gap of `gap_width` cells at one border (gap_left = true -> the
    // left/low-column border). Alternating gap sides on successive rungs makes a
    // serpentine. Grid row 0 = portal (outer); higher rows are nearer the nexus.
    void add_rung(int sector, int row, bool gap_left, int gap_width = 3,
                  uint8_t height = 2);

    // Build a continuous Archimedean spiral wall across ALL sectors: a single
    // smooth spiral (radius decreases by `pitch` per turn) walls every grid edge
    // it crosses and joins across the seams, leaving one smooth spiral corridor
    // that winds from the outer edge to the Nexus. `dir` = +1 CW, -1 CCW.
    void add_perfect_spiral(double pitch, int dir = +1, uint8_t height = 2);

private:
    game::GameMap map_;
    game::FlowFieldManager fields_;
    std::vector<Demon> demons_;
    std::mt19937_64 rng_;
    uint64_t tick_ = 0;
    uint32_t next_demon_id_ = 1;
    int reached_nexus_count_ = 0;

    // Cached boundary pairs between adjacent sectors (a*256+b -> pairs).
    std::unordered_map<int, std::vector<game::BoundaryPair>> boundary_cache_;
    const std::vector<game::BoundaryPair>& boundary_pairs(int a, int b);

    std::vector<BoundaryWall> boundary_walls_;
    // Is the crossing at `index` between a and b sealed by an "along" wall?
    bool crossing_blocked(int a, int b, int index) const;

    void step_demon(Demon& d, double dt);
    int target_sector_for(const Demon& d) const; // which crystal-sector it seeks

    friend std::vector<uint8_t> serialize_world(const World&);
    friend bool deserialize_world(World&, std::span<const uint8_t>);
};

} // namespace mad::core
