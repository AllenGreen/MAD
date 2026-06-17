#pragma once

#include "game/flow_field.hpp"
#include "game/game_map.hpp"
#include "game/global_flow_field.hpp"
#include "game/grid_types.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace mad::game {

// Owns and lazily builds the flow fields demons steer along. Fields are
// per-sector (the "Option B" architecture): one toward each sector's own
// crystal, one toward each sector's nexus row, and on-demand fields toward the
// boundary shared with a neighbor sector (used to hop around the ring).
//
// Every field is specific to a (MoveType, unit_size) pair. Results are cached;
// call invalidate_all() after any structural change (wall/tower placement).
class FlowFieldManager {
public:
    explicit FlowFieldManager(const GameMap& map) : map_(map) {}

    enum class Goal : uint8_t { Crystal = 0, Nexus = 1, Boundary = 2 };

    // Field guiding units in `sector` toward that sector's crystal.
    const FlowField& crystal_field(int sector, MoveType mt, int size);
    // Field guiding units in `sector` toward that sector's nexus row.
    const FlowField& nexus_field(int sector, MoveType mt, int size);
    // Field guiding units in `sector` toward its boundary with `neighbor`.
    const FlowField& boundary_field(int sector, int neighbor, MoveType mt, int size);

    // Whole-map fields used for demon steering (flood across seams).
    const GlobalFlowField& global_crystal_field(int target_sector, MoveType mt, int size);
    const GlobalFlowField& global_nexus_field(MoveType mt, int size);

    void invalidate_all() { fields_.clear(); global_fields_.clear(); }
    std::size_t cached_count() const { return fields_.size(); }

    // Mark a crossing position (index along boundary_cells(a,b)) blocked/unblocked
    // by an "along" boundary wall. Blocked positions are excluded from boundary
    // field goals, so demons route to an open crossing. Caller invalidates after.
    void set_boundary_blocked(int a, int b, int index, bool blocked);
    bool boundary_blocked(int a, int b, int index) const;

private:
    const GameMap& map_;
    std::unordered_map<uint64_t, FlowField> fields_;
    std::unordered_map<uint64_t, std::unique_ptr<GlobalFlowField>> global_fields_;
    // canonical (min,max) sector pair -> per-index blocked flags
    std::unordered_map<int, std::vector<char>> blocked_crossings_;

    static int pair_key(int a, int b) { return a < b ? a * 1000 + b : b * 1000 + a; }

    static uint64_t key(int sector, Goal goal, int neighbor, MoveType mt, int size) {
        return (static_cast<uint64_t>(sector) & 0xFF)
             | (static_cast<uint64_t>(goal) << 8)
             | (static_cast<uint64_t>(neighbor & 0xFF) << 16)
             | (static_cast<uint64_t>(static_cast<uint8_t>(mt)) << 24)
             | (static_cast<uint64_t>(size & 0xFF) << 32);
    }

    const FlowField& build(uint64_t k, int sector, Goal goal, int neighbor,
                           MoveType mt, int size);
};

} // namespace mad::game
