#include "game/grid.hpp"
#include "game/wall.hpp"

#include <cmath>
#include <memory>

namespace mad::game {

Grid::Grid(int width, int height)
    : width_(width)
    , height_(height)
    , cells_(static_cast<std::size_t>(width * height), CellState::Empty)
    , walls_(std::make_unique<WallSet>())
{}

bool Grid::in_bounds(CellCoord c) const {
    return c.col >= 0 && c.col < width_ && c.row >= 0 && c.row < height_;
}

CellState Grid::cell_state(CellCoord c) const {
    if (!in_bounds(c)) return CellState::Blocked;
    return cells_[index(c)];
}

void Grid::set_cell_state(CellCoord c, CellState state) {
    if (in_bounds(c)) {
        cells_[index(c)] = state;
    }
}

bool Grid::is_walkable(CellCoord c) const {
    if (!in_bounds(c)) return false;
    auto s = cells_[index(c)];
    return s == CellState::Empty || s == CellState::Boundary;
}

bool Grid::walls_block_move(CellCoord from, CellCoord to) const {
    int dc = to.col - from.col;
    int dr = to.row - from.row;

    // Cardinal moves: check the single edge between the cells
    if (dc == 0 && dr == -1) {
        // Moving north: horizontal edge on top of 'from' = bottom of 'to'
        return walls_->blocks_ground({from, EdgeType::Horizontal});
    }
    if (dc == 0 && dr == 1) {
        // Moving south: horizontal edge on top of 'to'
        return walls_->blocks_ground({to, EdgeType::Horizontal});
    }
    if (dc == -1 && dr == 0) {
        // Moving west: vertical edge on left of 'from'
        return walls_->blocks_ground({from, EdgeType::Vertical});
    }
    if (dc == 1 && dr == 0) {
        // Moving east: vertical edge on left of 'to'
        return walls_->blocks_ground({to, EdgeType::Vertical});
    }

    // Diagonal moves: strict, SYMMETRIC no-corner-cutting. A wall is a solid
    // barrier, so a diagonal is blocked by its own diagonal edge OR by ANY of the
    // four cardinal edges meeting at the corner vertex the diagonal passes
    // through. Computing those four edges from the shared vertex gives the same
    // answer in both directions, so can_move(A,B) == can_move(B,A) -- essential
    // for flow fields (an asymmetric rule strands units at wall corners).
    if (dc != 0 && dr != 0) {
        auto H = [&](int c, int r) {
            return walls_->blocks_ground({{c, r}, EdgeType::Horizontal});
        };
        auto V = [&](int c, int r) {
            return walls_->blocks_ground({{c, r}, EdgeType::Vertical});
        };
        const int vcol = dc > 0 ? from.col + 1 : from.col; // vertical edges' column
        const int hrow = dr > 0 ? from.row + 1 : from.row; // horizontal edges' row
        if (V(vcol, from.row)) return true;        // from <-> (c+dc, r)
        if (V(vcol, from.row + dr)) return true;   // (c, r+dr) <-> to
        if (H(from.col, hrow)) return true;        // from <-> (c, r+dr)
        if (H(from.col + dc, hrow)) return true;   // (c+dc, r) <-> to
        // The diagonal edge itself (named from whichever cell owns it).
        if (dc > 0 && dr < 0) return walls_->blocks_ground({from, EdgeType::DiagNE});
        if (dc < 0 && dr < 0) return walls_->blocks_ground({from, EdgeType::DiagNW});
        if (dc > 0 && dr > 0) return walls_->blocks_ground({to, EdgeType::DiagNW});
        return walls_->blocks_ground({to, EdgeType::DiagNE}); // dc<0 && dr>0
    }

    return false; // not adjacent
}

bool Grid::can_move(CellCoord from, CellCoord to) const {
    if (!is_walkable(from) || !is_walkable(to)) return false;

    int dc = std::abs(to.col - from.col);
    int dr = std::abs(to.row - from.row);

    // Must be adjacent (8-directional)
    if (dc > 1 || dr > 1 || (dc == 0 && dr == 0)) return false;

    // For diagonal moves, also check that the two corner cells aren't both towers
    // (prevents clipping through diagonal tower gaps)
    if (dc == 1 && dr == 1) {
        CellCoord corner_a = {from.col, to.row};
        CellCoord corner_b = {to.col, from.row};
        if (cell_state(corner_a) == CellState::Tower &&
            cell_state(corner_b) == CellState::Tower) {
            return false;
        }
    }

    return !walls_block_move(from, to);
}

int Grid::walkable_neighbors(CellCoord cell, CellCoord out[8]) const {
    static constexpr int dirs[8][2] = {
        { 0, -1}, // N
        { 1, -1}, // NE
        { 1,  0}, // E
        { 1,  1}, // SE
        { 0,  1}, // S
        {-1,  1}, // SW
        {-1,  0}, // W
        {-1, -1}  // NW
    };

    int count = 0;
    for (auto& [dc, dr] : dirs) {
        CellCoord neighbor = {cell.col + dc, cell.row + dr};
        if (can_move(cell, neighbor)) {
            out[count++] = neighbor;
        }
    }
    return count;
}

// --- Multi-size unit support ---

bool Grid::is_footprint_walkable(CellCoord origin, int size) const {
    if (size == 1) return is_walkable(origin);
    for (int r = 0; r < size; ++r)
        for (int c = 0; c < size; ++c)
            if (!is_walkable({origin.col + c, origin.row + r}))
                return false;
    return true;
}

bool Grid::walls_block_move(CellCoord from, CellCoord to, int size) const {
    if (size == 1) return walls_block_move(from, to);

    int dc = to.col - from.col;
    int dr = to.row - from.row;
    int S = size;

    // Cardinal moves: check S edges along the leading edge
    if (dc == 0 && dr == -1) {
        // Moving north: check S horizontal edges along the top of 'from'
        for (int i = 0; i < S; ++i)
            if (walls_->blocks_ground({{from.col + i, from.row}, EdgeType::Horizontal}))
                return true;
        return false;
    }
    if (dc == 0 && dr == 1) {
        // Moving south: check S horizontal edges along the bottom of 'from' (= top of row from.row+S)
        for (int i = 0; i < S; ++i)
            if (walls_->blocks_ground({{from.col + i, from.row + S}, EdgeType::Horizontal}))
                return true;
        return false;
    }
    if (dc == -1 && dr == 0) {
        // Moving west: check S vertical edges along the left of 'from'
        for (int i = 0; i < S; ++i)
            if (walls_->blocks_ground({{from.col, from.row + i}, EdgeType::Vertical}))
                return true;
        return false;
    }
    if (dc == 1 && dr == 0) {
        // Moving east: check S vertical edges along the right of 'from' (= left of col from.col+S)
        for (int i = 0; i < S; ++i)
            if (walls_->blocks_ground({{from.col + S, from.row + i}, EdgeType::Vertical}))
                return true;
        return false;
    }

    // Diagonal moves: check leading edges on both axes + diagonal edges + corner-cutting
    if (dc == 1 && dr == -1) {
        // Moving NE
        for (int i = 0; i < S; ++i) {
            if (walls_->blocks_ground({{from.col + i, from.row}, EdgeType::Horizontal})) return true;
            if (walls_->blocks_ground({{from.col + S, from.row + i}, EdgeType::Vertical})) return true;
            if (walls_->blocks_ground({{from.col + i, from.row + i}, EdgeType::DiagNE})) return true;
        }
        // Corner-cutting: if ALL north edges AND ALL east edges are walled
        bool all_north = true, all_east = true;
        for (int i = 0; i < S; ++i) {
            if (!walls_->blocks_ground({{from.col + i, from.row}, EdgeType::Horizontal})) all_north = false;
            if (!walls_->blocks_ground({{from.col + S, from.row + i}, EdgeType::Vertical})) all_east = false;
        }
        if (all_north && all_east) return true;
        return false;
    }
    if (dc == -1 && dr == -1) {
        // Moving NW
        for (int i = 0; i < S; ++i) {
            if (walls_->blocks_ground({{from.col + i, from.row}, EdgeType::Horizontal})) return true;
            if (walls_->blocks_ground({{from.col, from.row + i}, EdgeType::Vertical})) return true;
            if (walls_->blocks_ground({{from.col + i, from.row + i}, EdgeType::DiagNW})) return true;
        }
        bool all_north = true, all_west = true;
        for (int i = 0; i < S; ++i) {
            if (!walls_->blocks_ground({{from.col + i, from.row}, EdgeType::Horizontal})) all_north = false;
            if (!walls_->blocks_ground({{from.col, from.row + i}, EdgeType::Vertical})) all_west = false;
        }
        if (all_north && all_west) return true;
        return false;
    }
    if (dc == 1 && dr == 1) {
        // Moving SE
        for (int i = 0; i < S; ++i) {
            if (walls_->blocks_ground({{from.col + i, from.row + S}, EdgeType::Horizontal})) return true;
            if (walls_->blocks_ground({{from.col + S, from.row + i}, EdgeType::Vertical})) return true;
            if (walls_->blocks_ground({{to.col + i, to.row + i}, EdgeType::DiagNW})) return true;
        }
        bool all_south = true, all_east = true;
        for (int i = 0; i < S; ++i) {
            if (!walls_->blocks_ground({{from.col + i, from.row + S}, EdgeType::Horizontal})) all_south = false;
            if (!walls_->blocks_ground({{from.col + S, from.row + i}, EdgeType::Vertical})) all_east = false;
        }
        if (all_south && all_east) return true;
        return false;
    }
    if (dc == -1 && dr == 1) {
        // Moving SW
        for (int i = 0; i < S; ++i) {
            if (walls_->blocks_ground({{from.col + i, from.row + S}, EdgeType::Horizontal})) return true;
            if (walls_->blocks_ground({{from.col, from.row + i}, EdgeType::Vertical})) return true;
            if (walls_->blocks_ground({{to.col + i, to.row + i}, EdgeType::DiagNE})) return true;
        }
        bool all_south = true, all_west = true;
        for (int i = 0; i < S; ++i) {
            if (!walls_->blocks_ground({{from.col + i, from.row + S}, EdgeType::Horizontal})) all_south = false;
            if (!walls_->blocks_ground({{from.col, from.row + i}, EdgeType::Vertical})) all_west = false;
        }
        if (all_south && all_west) return true;
        return false;
    }

    return false;
}

bool Grid::can_move(CellCoord from, CellCoord to, int size) const {
    if (size == 1) return can_move(from, to);

    if (!is_footprint_walkable(from, size) || !is_footprint_walkable(to, size))
        return false;

    int dc = std::abs(to.col - from.col);
    int dr = std::abs(to.row - from.row);

    // Must be adjacent (8-directional, one step)
    if (dc > 1 || dr > 1 || (dc == 0 && dr == 0)) return false;

    // For diagonal moves, check that the two intermediate footprints
    // aren't both completely blocked (extended corner-cutting)
    if (dc == 1 && dr == 1) {
        CellCoord corner_a = {from.col, to.row}; // horizontal-first intermediate
        CellCoord corner_b = {to.col, from.row};  // vertical-first intermediate
        if (!is_footprint_walkable(corner_a, size) &&
            !is_footprint_walkable(corner_b, size)) {
            return false;
        }
    }

    return !walls_block_move(from, to, size);
}

int Grid::walkable_neighbors(CellCoord cell, int size, CellCoord out[8]) const {
    static constexpr int dirs[8][2] = {
        { 0, -1}, { 1, -1}, { 1,  0}, { 1,  1},
        { 0,  1}, {-1,  1}, {-1,  0}, {-1, -1}
    };

    int count = 0;
    for (auto& [dc, dr] : dirs) {
        CellCoord neighbor = {cell.col + dc, cell.row + dr};
        if (can_move(cell, neighbor, size)) {
            out[count++] = neighbor;
        }
    }
    return count;
}

bool Grid::has_wall(EdgeCoord edge) const { return walls_->has(edge); }
void Grid::add_wall(EdgeCoord edge) { walls_->add(edge); }
void Grid::remove_wall(EdgeCoord edge) { walls_->remove(edge); }
WallSet& Grid::walls() { return *walls_; }
const WallSet& Grid::walls() const { return *walls_; }

} // namespace mad::game
