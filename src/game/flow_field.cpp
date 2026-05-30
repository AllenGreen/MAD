#include "game/flow_field.hpp"
#include "game/grid_types.hpp"

#include <queue>
#include <cmath>

namespace mad::game {

static constexpr double SQRT2 = 1.4142135623730951;

FlowField::FlowField(const Grid& grid, int width, int height)
    : grid_(grid)
    , width_(width)
    , height_(height)
    , cost_field_(width * height, UNREACHABLE)
    , direction_field_(width * height, {-1, -1})
{}

void FlowField::generate(CellCoord goal, MoveType move_type, int unit_size) {
    std::array<CellCoord, 1> goals = {goal};
    generate(goals, move_type, unit_size);
}

void FlowField::generate(std::span<const CellCoord> goals, MoveType move_type, int unit_size) {
    unit_size_ = unit_size;
    std::fill(cost_field_.begin(), cost_field_.end(), UNREACHABLE);
    std::fill(direction_field_.begin(), direction_field_.end(), CellCoord{-1, -1});

    struct Entry {
        CellCoord cell;
        double cost;
        bool operator>(const Entry& o) const { return cost > o.cost; }
    };

    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;

    for (auto& goal : goals) {
        if (grid_.in_bounds(goal)) {
            cost_field_[index(goal)] = 0.0;
            open.push({goal, 0.0});
        }
    }

    static constexpr int dirs[8][2] = {
        { 0, -1}, { 1, -1}, { 1,  0}, { 1,  1},
        { 0,  1}, {-1,  1}, {-1,  0}, {-1, -1}
    };

    while (!open.empty()) {
        auto [cell, cost] = open.top();
        open.pop();

        if (cost > cost_field_[index(cell)]) continue;

        for (auto& [dc, dr] : dirs) {
            CellCoord neighbor{cell.col + dc, cell.row + dr};
            if (!grid_.in_bounds(neighbor)) continue;

            // Check movement based on move type and unit size
            bool passable = false;
            if (move_type == MoveType::Flyer) {
                passable = grid_.is_footprint_walkable(neighbor, unit_size);
            } else {
                passable = grid_.can_move(cell, neighbor, unit_size);
            }
            if (!passable) continue;

            double move_cost = (dc != 0 && dr != 0) ? SQRT2 : 1.0;
            double new_cost = cost + move_cost;

            int ni = index(neighbor);
            if (new_cost < cost_field_[ni]) {
                cost_field_[ni] = new_cost;
                open.push({neighbor, new_cost});
            }
        }
    }

    compute_directions();
    valid_ = true;
}

void FlowField::compute_directions() {
    static constexpr int dirs[8][2] = {
        { 0, -1}, { 1, -1}, { 1,  0}, { 1,  1},
        { 0,  1}, {-1,  1}, {-1,  0}, {-1, -1}
    };

    for (int r = 0; r < height_; ++r) {
        for (int c = 0; c < width_; ++c) {
            int ci = r * width_ + c;
            if (cost_field_[ci] == UNREACHABLE) continue;
            if (cost_field_[ci] == 0.0) {
                // At goal: direction is self
                direction_field_[ci] = {c, r};
                continue;
            }

            double best_cost = cost_field_[ci];
            CellCoord best_dir = {c, r}; // default: stay

            for (auto& [dc, dr] : dirs) {
                CellCoord neighbor{c + dc, r + dr};
                if (!grid_.in_bounds(neighbor)) continue;
                int ni = index(neighbor);
                if (cost_field_[ni] < best_cost) {
                    best_cost = cost_field_[ni];
                    best_dir = neighbor;
                }
            }

            direction_field_[ci] = best_dir;
        }
    }
}

CellCoord FlowField::best_neighbor(CellCoord cell) const {
    if (!grid_.in_bounds(cell)) return cell;
    return direction_field_[index(cell)];
}

double FlowField::cost_at(CellCoord cell) const {
    if (!grid_.in_bounds(cell)) return UNREACHABLE;
    return cost_field_[index(cell)];
}

} // namespace mad::game
