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
    // Walk along the boundary line between two adjacent sectors in world space.
    // The boundary is a line from the nexus (origin) outward along the shared angle.
    //
    // For each point along this line, find the nearest cell in each sector
    // and pair them if both are in-bounds and walkable.

    std::vector<BoundaryPair> pairs;

    // The boundary angle is halfway between the two sectors' rotations.
    double angle_a = sectors_[sector_a].rotation();
    double angle_b = sectors_[sector_b].rotation();

    // Handle wrap-around: find the angle between them
    double diff = angle_b - angle_a;
    while (diff > M_PI) diff -= 2.0 * M_PI;
    while (diff < -M_PI) diff += 2.0 * M_PI;
    double boundary_angle = angle_a + diff / 2.0;

    double cos_b = std::cos(boundary_angle);
    double sin_b = std::sin(boundary_angle);

    // Walk from nexus outward in steps of cell_size
    int steps = static_cast<int>(config_.map_radius / config_.cell_size) + 1;
    for (int i = 1; i <= steps; ++i) {
        double dist = i * config_.cell_size;
        // Point along boundary line (angle from +Y, clockwise)
        WorldPos point{dist * sin_b, dist * cos_b};

        CellCoord ca = sectors_[sector_a].world_to_cell(point);
        CellCoord cb = sectors_[sector_b].world_to_cell(point);

        if (sectors_[sector_a].grid().in_bounds(ca) &&
            sectors_[sector_b].grid().in_bounds(cb) &&
            sectors_[sector_a].grid().is_walkable(ca) &&
            sectors_[sector_b].grid().is_walkable(cb)) {
            // Avoid duplicate pairs
            if (pairs.empty() ||
                !(pairs.back().cell_a == ca && pairs.back().cell_b == cb)) {
                pairs.push_back({ca, cb});
            }
        }
    }

    return pairs;
}

std::vector<CellCoord> GameMap::portal_cells(int sector_id) const {
    return sectors_[sector_id].portal_cells();
}

std::vector<CellCoord> GameMap::nexus_cells(int sector_id) const {
    return sectors_[sector_id].nexus_cells();
}

} // namespace mad::game
