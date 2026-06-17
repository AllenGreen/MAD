#include "game/game_map.hpp"

#include <cmath>
#include <stdexcept>

namespace mad::game {

GameMap::GameMap(const MapConfig& config)
    : config_(config)
{
    init_sectors();
}

void GameMap::init_sectors() {
    sectors_.reserve(config_.num_players);
    double half_angle = M_PI / config_.num_players;

    for (int i = 0; i < config_.num_players; ++i) {
        double rotation = (2.0 * M_PI * i) / config_.num_players;
        sectors_.emplace_back(
            i,
            config_.grid_width,
            config_.grid_height,
            rotation,
            half_angle,
            config_.map_radius,
            config_.cell_size
        );
    }
}

Sector& GameMap::sector(int player_id) {
    return sectors_.at(player_id);
}

const Sector& GameMap::sector(int player_id) const {
    return sectors_.at(player_id);
}

int GameMap::sector_at(WorldPos pos) const {
    for (int i = 0; i < static_cast<int>(sectors_.size()); ++i) {
        if (sectors_[i].contains_world(pos))
            return i;
    }
    return -1;
}

CellCoord GameMap::transform_cell(CellCoord cell, int from_sector, int to_sector) const {
    // Cell in from_sector -> world -> cell in to_sector
    WorldPos world = sectors_[from_sector].cell_to_world(cell);
    return sectors_[to_sector].world_to_cell(world);
}

std::vector<BoundaryPair> GameMap::boundary_cells(int sector_a, int sector_b) const {
    // On the polar grid the seam between adjacent sectors is exactly a grid line:
    // sector a's edge column meets sector b's edge column, row for row (so each
    // pair is at the same radius). a's CW neighbour shares a's last column with the
    // neighbour's column 0; the CCW neighbour shares column 0 with column W-1.
    std::vector<BoundaryPair> pairs;
    const int N = num_sectors();
    if (N < 2) return pairs;
    const bool cw = (sector_a + 1) % N == sector_b;
    const bool ccw = (sector_a - 1 + N) % N == sector_b;
    if (!cw && !ccw) return pairs; // not adjacent
    const int W = config_.grid_width, H = config_.grid_height;
    const int col_a = cw ? W - 1 : 0;
    const int col_b = cw ? 0 : W - 1;
    for (int row = 0; row < H; ++row) {
        const CellCoord ca{col_a, row}, cb{col_b, row};
        if (sectors_[sector_a].grid().is_walkable(ca) &&
            sectors_[sector_b].grid().is_walkable(cb))
            pairs.push_back({ca, cb});
    }
    return pairs;
}

std::vector<CellCoord> GameMap::portal_cells(int sector_id) const {
    return sectors_[sector_id].portal_cells();
}

std::vector<CellCoord> GameMap::nexus_cells(int sector_id) const {
    return sectors_[sector_id].nexus_cells();
}

CellCoord GameMap::crystal_cell(int sector_id) const {
    return sectors_[sector_id].crystal_cell();
}

WorldPos GameMap::crystal_world(int sector_id) const {
    return sectors_[sector_id].crystal_world();
}

} // namespace mad::game
