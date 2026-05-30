#pragma once

#include "game/grid_types.hpp"
#include "game/grid.hpp"

#include <vector>

namespace mad::game {

class GameMap;
class Sector;

struct PathRequest {
    CellCoord start;
    CellCoord goal;
    int sector_id = 0;
    MoveType move_type = MoveType::Ground;
    int unit_size = 1;
    int max_steps = 10000;
};

struct PathResult {
    bool found = false;
    std::vector<CellCoord> cells;
    std::vector<WorldPos> waypoints; // string-pulled smooth path
    double cost = 0.0;
};

class Pathfinder {
public:
    explicit Pathfinder(GameMap& map);

    // A* within one sector
    PathResult find_path(const PathRequest& request);

    // Cross-sector path: chains through boundary pairs
    PathResult find_path_cross_sector(CellCoord start, int start_sector,
                                       CellCoord goal, int goal_sector,
                                       MoveType move_type);

    // String-pull a cell path into smooth waypoints (exposed for testing)
    std::vector<WorldPos> string_pull(const std::vector<CellCoord>& path,
                                       const Sector& sector,
                                       int unit_size = 1) const;

private:
    GameMap& map_;

    double heuristic(CellCoord a, CellCoord b) const;
    bool is_walkable(const Grid& grid, CellCoord cell, MoveType mt, int unit_size = 1) const;
    bool can_traverse(const Grid& grid, CellCoord from, CellCoord to, MoveType mt, int unit_size = 1) const;

    // Line-of-sight check on grid (for string-pulling)
    bool line_of_sight(const Grid& grid, CellCoord from, CellCoord to, int unit_size = 1) const;
};

} // namespace mad::game
