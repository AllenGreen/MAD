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
{
    // POLAR grid: each sector is its own wedge of a shared polar coordinate
    // system. Columns map to angle across the full wedge and rows to radius, so a
    // sector's right edge lines up exactly with its neighbour's left edge -- the
    // grid is continuous (a curved line) across every border. Every cell lies
    // inside the wedge by construction, so nothing needs masking.
    mask_out_of_wedge_cells();
}

// Column edge in [0, width] -> angle from +Y (clockwise). col 0 = the sector's
// left (CCW) border, col = width = the right (CW) border.
double Sector::angle_at(double col_edge) const {
    return rotation_ - half_angle_
         + (col_edge / grid_.width()) * (2.0 * half_angle_);
}

// Row edge in [0, height] -> radius from the nexus. row 0 = portal (map_radius),
// row = height = the nexus (radius 0).
double Sector::radius_at(double row_edge) const {
    return map_radius_ * (1.0 - row_edge / grid_.height());
}

WorldPos Sector::cell_to_world(CellCoord cell) const {
    const double a = angle_at(cell.col + 0.5);
    const double r = radius_at(cell.row + 0.5);
    return {r * std::sin(a), r * std::cos(a)};
}

CellCoord Sector::world_to_cell(WorldPos pos) const {
    const double r = std::sqrt(pos.x * pos.x + pos.y * pos.y);
    double da = std::atan2(pos.x, pos.y) - rotation_;
    while (da > M_PI) da -= 2.0 * M_PI;
    while (da < -M_PI) da += 2.0 * M_PI;
    const int col = static_cast<int>(
        std::floor((da + half_angle_) / (2.0 * half_angle_) * grid_.width()));
    const int row = static_cast<int>(
        std::floor((1.0 - r / map_radius_) * grid_.height()));
    return {col, row};
}

void Sector::cell_corners(CellCoord cell, WorldPos out[4]) const {
    const double a0 = angle_at(cell.col), a1 = angle_at(cell.col + 1);
    const double r0 = radius_at(cell.row), r1 = radius_at(cell.row + 1); // r0 outer
    out[0] = {r0 * std::sin(a0), r0 * std::cos(a0)}; // outer-left
    out[1] = {r0 * std::sin(a1), r0 * std::cos(a1)}; // outer-right
    out[2] = {r1 * std::sin(a1), r1 * std::cos(a1)}; // inner-right
    out[3] = {r1 * std::sin(a0), r1 * std::cos(a0)}; // inner-left
}

bool Sector::contains_world(WorldPos pos) const {
    const double dist = std::sqrt(pos.x * pos.x + pos.y * pos.y);
    if (dist > map_radius_ * 1.01) return false;
    if (dist < 1e-6) return true; // at the nexus, belongs to all sectors

    double diff = std::atan2(pos.x, pos.y) - rotation_;
    while (diff > M_PI) diff -= 2.0 * M_PI;
    while (diff < -M_PI) diff += 2.0 * M_PI;
    return std::abs(diff) <= half_angle_ + 1e-6;
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
    // Polar grid: every cell already lies inside the wedge, so nothing is masked.
    // (Kept for API compatibility / future irregular maps.)
    for (int r = 0; r < grid_.height(); ++r)
        for (int c = 0; c < grid_.width(); ++c) {
            CellCoord cell{c, r};
            if (!contains_world(cell_to_world(cell)))
                grid_.set_cell_state(cell, CellState::Blocked);
        }
}

} // namespace mad::game
