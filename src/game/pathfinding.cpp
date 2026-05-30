#include "game/pathfinding.hpp"
#include "game/game_map.hpp"
#include "game/sector.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <vector>

namespace mad::game {

static constexpr double SQRT2 = 1.4142135623730951;
static constexpr double SMASHER_WALL_PENALTY = 5.0;

Pathfinder::Pathfinder(GameMap& map)
    : map_(map) {}

double Pathfinder::heuristic(CellCoord a, CellCoord b) const {
    int dx = std::abs(a.col - b.col);
    int dy = std::abs(a.row - b.row);
    return std::max(dx, dy) + (SQRT2 - 1.0) * std::min(dx, dy);
}

bool Pathfinder::is_walkable(const Grid& grid, CellCoord cell, MoveType mt, int unit_size) const {
    if (unit_size == 1) {
        if (!grid.in_bounds(cell)) return false;
        auto state = grid.cell_state(cell);
        if (mt == MoveType::Flyer) return state != CellState::Blocked;
        return state == CellState::Empty || state == CellState::Boundary;
    }

    // Multi-size: check entire footprint
    for (int r = 0; r < unit_size; ++r) {
        for (int c = 0; c < unit_size; ++c) {
            CellCoord fc{cell.col + c, cell.row + r};
            if (!grid.in_bounds(fc)) return false;
            auto state = grid.cell_state(fc);
            if (mt == MoveType::Flyer) {
                if (state == CellState::Blocked) return false;
            } else {
                if (state != CellState::Empty && state != CellState::Boundary) return false;
            }
        }
    }
    return true;
}

bool Pathfinder::can_traverse(const Grid& grid, CellCoord from, CellCoord to, MoveType mt, int unit_size) const {
    if (!is_walkable(grid, to, mt, unit_size)) return false;

    // Flyers ignore walls entirely
    if (mt == MoveType::Flyer) return true;

    return grid.can_move(from, to, unit_size);
}

PathResult Pathfinder::find_path(const PathRequest& request) {
    auto& sector = map_.sector(request.sector_id);
    auto& grid = sector.grid();
    int sz = request.unit_size;

    PathResult result;

    if (!is_walkable(grid, request.start, request.move_type, sz) ||
        !is_walkable(grid, request.goal, request.move_type, sz)) {
        return result;
    }

    if (request.start == request.goal) {
        result.found = true;
        result.cells.push_back(request.start);
        result.waypoints.push_back(sector.cell_to_world(request.start));
        result.cost = 0.0;
        return result;
    }

    struct Node {
        CellCoord cell;
        double g;
        double f;
    };

    auto cmp = [](const Node& a, const Node& b) { return a.f > b.f; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);

    int w = grid.width();
    int h = grid.height();
    std::vector<double> g_cost(w * h, std::numeric_limits<double>::infinity());
    std::vector<bool> closed(w * h, false);
    std::vector<CellCoord> parent(w * h, {-1, -1});

    auto idx = [w](CellCoord c) { return c.row * w + c.col; };

    g_cost[idx(request.start)] = 0.0;
    open.push({request.start, 0.0, heuristic(request.start, request.goal)});

    int steps = 0;
    bool found = false;

    static constexpr int dirs[8][2] = {
        { 0, -1}, { 1, -1}, { 1,  0}, { 1,  1},
        { 0,  1}, {-1,  1}, {-1,  0}, {-1, -1}
    };

    while (!open.empty() && steps < request.max_steps) {
        auto current = open.top();
        open.pop();
        ++steps;

        if (current.cell == request.goal) {
            found = true;
            break;
        }

        int ci = idx(current.cell);
        if (closed[ci]) continue;
        closed[ci] = true;

        for (auto& [dc, dr] : dirs) {
            CellCoord neighbor{current.cell.col + dc, current.cell.row + dr};
            if (!grid.in_bounds(neighbor)) continue;

            int ni = idx(neighbor);
            if (closed[ni]) continue;

            if (!can_traverse(grid, current.cell, neighbor, request.move_type, sz))
                continue;

            double move_cost = (dc != 0 && dr != 0) ? SQRT2 : 1.0;

            // Smasher penalty for walls
            if (request.move_type == MoveType::Smasher) {
                if (!grid.can_move(current.cell, neighbor, sz)) {
                    move_cost += SMASHER_WALL_PENALTY;
                }
            }

            double new_g = current.g + move_cost;
            if (new_g < g_cost[ni]) {
                g_cost[ni] = new_g;
                parent[ni] = current.cell;
                open.push({neighbor, new_g, new_g + heuristic(neighbor, request.goal)});
            }
        }
    }

    if (!found) return result;

    // Reconstruct path
    result.found = true;
    result.cost = g_cost[idx(request.goal)];

    CellCoord cell = request.goal;
    while (!(cell == request.start)) {
        result.cells.push_back(cell);
        cell = parent[idx(cell)];
    }
    result.cells.push_back(request.start);
    std::reverse(result.cells.begin(), result.cells.end());

    // String-pull for smooth waypoints
    result.waypoints = string_pull(result.cells, sector, sz);

    return result;
}

PathResult Pathfinder::find_path_cross_sector(
    CellCoord start, int start_sector,
    CellCoord goal, int goal_sector,
    MoveType move_type)
{
    if (start_sector == goal_sector) {
        return find_path({start, goal, start_sector, move_type});
    }

    auto pairs = map_.boundary_cells(start_sector, goal_sector);
    if (pairs.empty()) {
        return {};
    }

    PathResult best;
    best.cost = std::numeric_limits<double>::infinity();

    for (auto& [ba, bb] : pairs) {
        auto path_a = find_path({start, ba, start_sector, move_type});
        if (!path_a.found) continue;

        auto path_b = find_path({bb, goal, goal_sector, move_type});
        if (!path_b.found) continue;

        double total = path_a.cost + path_b.cost + 1.0;
        if (total < best.cost) {
            best.found = true;
            best.cost = total;
            best.cells = std::move(path_a.cells);
            for (std::size_t i = 0; i < path_b.cells.size(); ++i) {
                best.cells.push_back(path_b.cells[i]);
            }
            best.waypoints = std::move(path_a.waypoints);
            for (auto& wp : path_b.waypoints) {
                best.waypoints.push_back(wp);
            }
        }
    }

    return best;
}

bool Pathfinder::line_of_sight(const Grid& grid, CellCoord from, CellCoord to, int unit_size) const {
    int dx = to.col - from.col;
    int dy = to.row - from.row;
    int sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    int sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
    dx = std::abs(dx);
    dy = std::abs(dy);

    CellCoord current = from;

    if (dx >= dy) {
        int err = dx / 2;
        for (int i = 0; i < dx; ++i) {
            CellCoord next = current;
            next.col += sx;
            err -= dy;
            if (err < 0) {
                next.row += sy;
                err += dx;
                if (!grid.can_move(current, next, unit_size)) return false;
            } else {
                if (!grid.can_move(current, next, unit_size)) return false;
            }
            current = next;
        }
    } else {
        int err = dy / 2;
        for (int i = 0; i < dy; ++i) {
            CellCoord next = current;
            next.row += sy;
            err -= dx;
            if (err < 0) {
                next.col += sx;
                err += dy;
                if (!grid.can_move(current, next, unit_size)) return false;
            } else {
                if (!grid.can_move(current, next, unit_size)) return false;
            }
            current = next;
        }
    }

    return true;
}

std::vector<WorldPos> Pathfinder::string_pull(
    const std::vector<CellCoord>& path,
    const Sector& sector,
    int unit_size) const
{
    if (path.empty()) return {};
    if (path.size() == 1) {
        return {sector.cell_to_world(path[0])};
    }

    auto& grid = sector.grid();
    std::vector<WorldPos> waypoints;
    waypoints.push_back(sector.cell_to_world(path[0]));

    std::size_t anchor = 0;
    while (anchor < path.size() - 1) {
        std::size_t furthest = anchor + 1;
        for (std::size_t i = anchor + 2; i < path.size(); ++i) {
            if (line_of_sight(grid, path[anchor], path[i], unit_size)) {
                furthest = i;
            }
        }
        waypoints.push_back(sector.cell_to_world(path[furthest]));
        anchor = furthest;
    }

    return waypoints;
}

} // namespace mad::game
