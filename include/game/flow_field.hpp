#pragma once

#include "game/grid_types.hpp"
#include "game/grid.hpp"

#include <span>
#include <vector>
#include <limits>

namespace mad::game {

class FlowField {
public:
    FlowField(const Grid& grid, int width, int height);

    // Generate flow field toward a single goal
    void generate(CellCoord goal, MoveType move_type, int unit_size = 1);

    // Generate flow field toward multiple goals
    void generate(std::span<const CellCoord> goals, MoveType move_type, int unit_size = 1);

    // Best neighbor to move toward goal. Returns cell itself if unreachable.
    CellCoord best_neighbor(CellCoord cell) const;

    // Cost from this cell to nearest goal
    double cost_at(CellCoord cell) const;

    bool is_valid() const { return valid_; }
    void invalidate() { valid_ = false; }

    static constexpr double UNREACHABLE = std::numeric_limits<double>::infinity();

private:
    const Grid& grid_;
    int width_, height_;
    std::vector<double> cost_field_;
    std::vector<CellCoord> direction_field_;
    bool valid_ = false;
    int unit_size_ = 1;

    int index(CellCoord c) const { return c.row * width_ + c.col; }
    void compute_directions();
};

} // namespace mad::game
