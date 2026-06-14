#pragma once

#include "game/grid.hpp"
#include "game/grid_types.hpp"

#include <cmath>
#include <vector>

namespace mad::game {

class Sector {
public:
    // rotation_rad: angle of this sector's center line from +Y axis (clockwise)
    // half_angle: half the angular width of the sector (pi/N for N players)
    Sector(int player_id, int grid_width, int grid_height,
           double rotation_rad, double half_angle,
           double map_radius, double cell_size);

    int player_id() const { return player_id_; }
    Grid& grid() { return grid_; }
    const Grid& grid() const { return grid_; }
    double rotation() const { return rotation_; }
    double half_angle() const { return half_angle_; }
    double cell_size() const { return cell_size_; }

    // Transform: grid cell center -> world position
    WorldPos cell_to_world(CellCoord cell) const;

    // Transform: world position -> nearest grid cell (may be out of bounds)
    CellCoord world_to_cell(WorldPos pos) const;

    // Is this world position inside this sector's wedge?
    bool contains_world(WorldPos pos) const;

    // Portal cells: row 0 cells that are walkable
    std::vector<CellCoord> portal_cells() const;

    // Nexus cells: last-row cells that are walkable
    std::vector<CellCoord> nexus_cells() const;

    // Summoning Crystal: mid-wedge position. Centered column, mid-depth row.
    CellCoord crystal_cell() const;
    WorldPos crystal_world() const;

    // Mark cells outside the wedge polygon as Blocked
    void mask_out_of_wedge_cells();

private:
    int player_id_;
    double rotation_;
    double half_angle_;
    double map_radius_;
    double cell_size_;
    Grid grid_;

    // Cached transform: cos/sin of rotation
    double cos_r_, sin_r_;

    // Grid origin in world space (top-left of the grid before rotation)
    // The grid is laid out so that:
    //   - Row 0 is at the outer edge (portal), last row toward nexus
    //   - Col 0 is the left edge, last col is the right edge
    //   - The grid center-top aligns with the sector center line at the portal distance

    // Local-to-world: local position is grid-relative (col * cell_size, row * cell_size)
    // where (0,0) is top-left of the grid.
    // The grid is centered on the sector's center line, with row 0 at map_radius distance.
    WorldPos local_to_world(double lx, double ly) const;
    void world_to_local(WorldPos pos, double& lx, double& ly) const;
};

} // namespace mad::game
