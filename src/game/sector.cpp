#include "game/sector.hpp"

#include <algorithm>
#include <cmath>

namespace mad::game {

Sector::Sector(int player_id, int grid_width, int grid_height,
               double rotation_rad, double half_angle,
               double map_radius, double cell_size)
    : player_id_(player_id)
    , rotation_(rotation_rad)
    , half_angle_(half_angle)
    , map_radius_(map_radius)
    , cell_size_(cell_size)
    , grid_(grid_width, grid_height)
    , cos_r_(std::cos(rotation_rad))
    , sin_r_(std::sin(rotation_rad))
{
    mask_out_of_wedge_cells();
}

WorldPos Sector::local_to_world(double lx, double ly) const {
    // Local coordinate system:
    //   lx: horizontal, 0 = left edge of grid, increases right
    //   ly: vertical, 0 = top (portal edge), increases toward nexus (center)
    //
    // In the "unrotated" sector (player 0, facing up from the top):
    //   The sector's center line goes from (0, map_radius) down to (0, 0) [nexus].
    //   Grid top-left in unrotated space:
    //     x = -grid_width * cell_size / 2  (centered horizontally)
    //     y = map_radius                    (portal at the outer edge)
    //   Grid y decreases toward nexus (ly increases -> world y decreases).

    double grid_w = grid_.width() * cell_size_;

    // Unrotated position
    double ux = lx - grid_w / 2.0;
    double uy = map_radius_ - ly; // row 0 at map_radius, last row toward 0

    // Rotate by sector angle (clockwise rotation around origin/nexus)
    return {
        ux * cos_r_ + uy * sin_r_,
       -ux * sin_r_ + uy * cos_r_
    };
}

void Sector::world_to_local(WorldPos pos, double& lx, double& ly) const {
    // Inverse rotation
    double ux = pos.x * cos_r_ - pos.y * sin_r_;
    double uy = pos.x * sin_r_ + pos.y * cos_r_;

    double grid_w = grid_.width() * cell_size_;
    lx = ux + grid_w / 2.0;
    ly = map_radius_ - uy;
}

WorldPos Sector::cell_to_world(CellCoord cell) const {
    double lx = (cell.col + 0.5) * cell_size_;
    double ly = (cell.row + 0.5) * cell_size_;
    return local_to_world(lx, ly);
}

CellCoord Sector::world_to_cell(WorldPos pos) const {
    double lx, ly;
    world_to_local(pos, lx, ly);
    return {
        static_cast<int>(std::floor(lx / cell_size_)),
        static_cast<int>(std::floor(ly / cell_size_))
    };
}

bool Sector::contains_world(WorldPos pos) const {
    // A point is in this sector if its angle from +Y axis (clockwise)
    // is within [rotation_ - half_angle_, rotation_ + half_angle_].
    // Also must be within map_radius distance from origin.

    double dist = std::sqrt(pos.x * pos.x + pos.y * pos.y);
    if (dist > map_radius_ * 1.01) return false; // small tolerance
    if (dist < 1e-6) return true; // at nexus center, belongs to all sectors

    // Angle from +Y axis, clockwise (matching our rotation convention)
    double angle = std::atan2(pos.x, pos.y);

    // Normalize angle difference to [-pi, pi]
    double diff = angle - rotation_;
    while (diff > M_PI) diff -= 2.0 * M_PI;
    while (diff < -M_PI) diff += 2.0 * M_PI;

    return std::abs(diff) <= half_angle_ + 1e-6; // small tolerance
}

std::vector<CellCoord> Sector::portal_cells() const {
    std::vector<CellCoord> cells;
    for (int c = 0; c < grid_.width(); ++c) {
        CellCoord coord{c, 0};
        if (grid_.is_walkable(coord))
            cells.push_back(coord);
    }
    return cells;
}

CellCoord Sector::crystal_cell() const {
    return {grid_.width() / 2, grid_.height() / 2};
}

WorldPos Sector::crystal_world() const {
    return cell_to_world(crystal_cell());
}

std::vector<CellCoord> Sector::nexus_cells() const {
    std::vector<CellCoord> cells;
    int last_row = grid_.height() - 1;
    for (int c = 0; c < grid_.width(); ++c) {
        CellCoord coord{c, last_row};
        if (grid_.is_walkable(coord))
            cells.push_back(coord);
    }
    return cells;
}

void Sector::mask_out_of_wedge_cells() {
    for (int r = 0; r < grid_.height(); ++r) {
        for (int c = 0; c < grid_.width(); ++c) {
            CellCoord cell{c, r};
            WorldPos wp = cell_to_world(cell);
            if (!contains_world(wp)) {
                grid_.set_cell_state(cell, CellState::Blocked);
            }
        }
    }
}

} // namespace mad::game
