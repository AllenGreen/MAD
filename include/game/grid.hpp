#pragma once

#include "game/grid_types.hpp"
#include "game/wall.hpp"
#include <memory>
#include <vector>

namespace mad::game {

class Grid {
public:
    Grid(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    bool in_bounds(CellCoord c) const;

    CellState cell_state(CellCoord c) const;
    void set_cell_state(CellCoord c, CellState state);

    // Is a cell walkable (Empty or Boundary)?
    bool is_walkable(CellCoord c) const;

    // Is the full size x size footprint starting at origin walkable?
    bool is_footprint_walkable(CellCoord origin, int size) const;

    // Can a ground unit move from cell A to adjacent cell B?
    // Checks cell walkability and wall edges.
    bool can_move(CellCoord from, CellCoord to) const;
    bool can_move(CellCoord from, CellCoord to, int size) const;

    // Get walkable neighbors (8-directional). Returns count written to out[].
    int walkable_neighbors(CellCoord cell, CellCoord out[8]) const;
    int walkable_neighbors(CellCoord cell, int size, CellCoord out[8]) const;

    // Wall access (delegates to internal WallSet)
    bool has_wall(EdgeCoord edge) const;
    void add_wall(EdgeCoord edge);
    void remove_wall(EdgeCoord edge);
    WallSet& walls();
    const WallSet& walls() const;

private:
    int width_;
    int height_;
    std::vector<CellState> cells_; // row-major: cells_[row * width_ + col]
    std::unique_ptr<WallSet> walls_;

    int index(CellCoord c) const { return c.row * width_ + c.col; }

    // Check if wall edges block movement between two adjacent cells
    bool walls_block_move(CellCoord from, CellCoord to) const;
    bool walls_block_move(CellCoord from, CellCoord to, int size) const;
};

} // namespace mad::game
