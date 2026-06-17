#pragma once

#include "game/game_map.hpp"
#include "game/grid_types.hpp"

#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace mad::game {

// A flow field that spans the WHOLE map: it floods (Dijkstra) across every
// sector's grid AND across the open seam crossings between adjacent sectors, so
// the resulting path is the globally shortest one -- it can wind tangentially
// around the ring and across seams, which a per-sector field cannot express.
//
// The steering direction is recorded during relaxation (each node points back to
// the node it was reached from, which is strictly closer to the goal), so the
// direction is always a legal move/crossing by construction -- no separate,
// wall-ignoring direction pass.
class GlobalFlowField {
public:
    explicit GlobalFlowField(const GameMap& map) : map_(map) {}

    static constexpr double UNREACHABLE = std::numeric_limits<double>::infinity();

    // One step toward the goal: the cell to move to and which sector it's in.
    struct Step {
        int sector = -1;
        CellCoord cell{};
    };

    // crossing_blocked(a, b, index) -> true if the seam crossing is sealed.
    using BlockedFn = std::function<bool(int, int, int)>;

    void generate(const std::vector<std::pair<int, CellCoord>>& goals,
                  MoveType mt, int size, const BlockedFn& crossing_blocked);

    // Next step toward the goal from (sector, cell). Returns the cell itself if
    // it is the goal or unreachable.
    Step step_at(int sector, CellCoord cell) const;
    double cost_at(int sector, CellCoord cell) const;
    bool is_valid() const { return valid_; }

private:
    const GameMap& map_;
    int n_ = 0, width_ = 0, height_ = 0;
    std::vector<std::vector<double>> cost_;  // [sector][row*width+col]
    std::vector<std::vector<Step>> dir_;     // step toward goal
    bool valid_ = false;

    int idx(CellCoord c) const { return c.row * width_ + c.col; }
};

} // namespace mad::game
