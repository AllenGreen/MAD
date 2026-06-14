#pragma once

#include "game/sector.hpp"
#include "game/grid_types.hpp"

#include <vector>

namespace mad::game {

struct MapConfig {
    int num_players = 2;
    int grid_width = 20;   // cells wide per sector
    int grid_height = 30;  // cells deep (portal to nexus)
    double map_radius = 50.0; // world units from center to portal midpoint
    double cell_size = 1.0;
};

struct BoundaryPair {
    CellCoord cell_a; // in sector a's grid
    CellCoord cell_b; // in sector b's grid
};

class GameMap {
public:
    explicit GameMap(const MapConfig& config);

    int num_sectors() const { return static_cast<int>(sectors_.size()); }
    Sector& sector(int player_id);
    const Sector& sector(int player_id) const;
    const MapConfig& config() const { return config_; }

    // Which sector contains this world position? Returns player_id, or -1.
    int sector_at(WorldPos pos) const;

    // Convert a cell from one sector to the nearest cell in another.
    CellCoord transform_cell(CellCoord cell, int from_sector, int to_sector) const;

    // Get paired boundary cells between two adjacent sectors.
    std::vector<BoundaryPair> boundary_cells(int sector_a, int sector_b) const;

    // Portal cells for a sector (row 0, walkable)
    std::vector<CellCoord> portal_cells(int sector_id) const;

    // Nexus cells for a sector (last row, walkable)
    std::vector<CellCoord> nexus_cells(int sector_id) const;

    // Summoning Crystal: per-sector mid-wedge cell and world position.
    CellCoord crystal_cell(int sector_id) const;
    WorldPos crystal_world(int sector_id) const;

private:
    MapConfig config_;
    std::vector<Sector> sectors_;

    void init_sectors();
};

} // namespace mad::game
