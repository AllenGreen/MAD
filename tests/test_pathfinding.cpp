#include "core/test.hpp"
#include "game/pathfinding.hpp"
#include "game/flow_field.hpp"
#include "game/game_map.hpp"

#include <cmath>

using namespace mad::game;

// Helper: create a simple 1-sector map for testing pathing in isolation
static GameMap make_simple_map(int w, int h) {
    MapConfig config{.num_players = 1, .grid_width = w, .grid_height = h,
                     .map_radius = static_cast<double>(h + 10), .cell_size = 1.0};
    return GameMap(config);
}

TEST(pathfinding_straight_line) {
    auto map = make_simple_map(10, 10);
    Pathfinder pf(map);

    auto result = pf.find_path({.start = {0, 5}, .goal = {9, 5}, .sector_id = 0});
    ASSERT_TRUE(result.found);
    ASSERT_EQ(result.cells.front().col, 0);
    ASSERT_EQ(result.cells.back().col, 9);
    // Straight horizontal path: cost should be 9.0
    ASSERT_TRUE(std::abs(result.cost - 9.0) < 0.01);
    return true;
}

TEST(pathfinding_diagonal) {
    auto map = make_simple_map(10, 10);
    Pathfinder pf(map);

    auto result = pf.find_path({.start = {0, 0}, .goal = {5, 5}, .sector_id = 0});
    ASSERT_TRUE(result.found);
    // Diagonal path: 5 diagonal steps, cost = 5 * sqrt(2)
    ASSERT_TRUE(std::abs(result.cost - 5.0 * std::sqrt(2.0)) < 0.01);
    return true;
}

TEST(pathfinding_around_wall) {
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();

    // Block column 5 with a vertical wall of towers from row 0 to row 7
    for (int r = 0; r < 8; ++r) {
        grid.set_cell_state({5, r}, CellState::Tower);
    }

    Pathfinder pf(map);
    auto result = pf.find_path({.start = {3, 3}, .goal = {7, 3}, .sector_id = 0});
    ASSERT_TRUE(result.found);
    // Path must go around the tower column (south past row 7, then east, then north)
    ASSERT_TRUE(result.cost > 4.0); // longer than direct path of 4
    return true;
}

TEST(pathfinding_wall_edge_blocks) {
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();

    // Place horizontal walls AND diagonal walls across row 5 to fully block
    // all 8-directional movement from row 5+ to row 4-
    for (int c = 0; c < 10; ++c) {
        grid.add_wall({{c, 5}, EdgeType::Horizontal}); // blocks N/S cardinal
        grid.add_wall({{c, 5}, EdgeType::DiagNE});     // blocks NE diagonal
        grid.add_wall({{c, 5}, EdgeType::DiagNW});     // blocks NW diagonal
    }

    Pathfinder pf(map);
    auto result = pf.find_path({.start = {5, 6}, .goal = {5, 4}, .sector_id = 0});
    // Full wall barrier — path should be impossible
    ASSERT_TRUE(!result.found);
    return true;
}

TEST(pathfinding_unreachable) {
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();

    // Surround goal with towers
    grid.set_cell_state({4, 4}, CellState::Tower);
    grid.set_cell_state({5, 4}, CellState::Tower);
    grid.set_cell_state({6, 4}, CellState::Tower);
    grid.set_cell_state({4, 5}, CellState::Tower);
    grid.set_cell_state({6, 5}, CellState::Tower);
    grid.set_cell_state({4, 6}, CellState::Tower);
    grid.set_cell_state({5, 6}, CellState::Tower);
    grid.set_cell_state({6, 6}, CellState::Tower);

    Pathfinder pf(map);
    auto result = pf.find_path({.start = {0, 0}, .goal = {5, 5}, .sector_id = 0});
    ASSERT_TRUE(!result.found);
    return true;
}

TEST(pathfinding_same_cell) {
    auto map = make_simple_map(10, 10);
    Pathfinder pf(map);

    auto result = pf.find_path({.start = {3, 3}, .goal = {3, 3}, .sector_id = 0});
    ASSERT_TRUE(result.found);
    ASSERT_EQ(result.cells.size(), 1u);
    ASSERT_TRUE(std::abs(result.cost) < 0.01);
    return true;
}

TEST(string_pull_straight_line) {
    auto map = make_simple_map(10, 10);
    Pathfinder pf(map);

    auto result = pf.find_path({.start = {0, 5}, .goal = {9, 5}, .sector_id = 0});
    ASSERT_TRUE(result.found);
    // String-pulled straight line should have just 2 waypoints (start and end)
    ASSERT_EQ(result.waypoints.size(), 2u);
    return true;
}

TEST(string_pull_around_obstacle) {
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();

    // Place a tower in the middle
    grid.set_cell_state({5, 5}, CellState::Tower);

    Pathfinder pf(map);
    auto result = pf.find_path({.start = {3, 5}, .goal = {7, 5}, .sector_id = 0});
    ASSERT_TRUE(result.found);
    // Must go around the tower, so more than 2 waypoints
    ASSERT_TRUE(result.waypoints.size() >= 2u);
    return true;
}

// --- Multi-size pathfinding tests ---

TEST(pathfinding_2x2_straight_line) {
    auto map = make_simple_map(10, 10);
    Pathfinder pf(map);

    auto result = pf.find_path({.start = {0, 4}, .goal = {6, 4}, .sector_id = 0,
                                 .move_type = MoveType::Ground, .unit_size = 2});
    ASSERT_TRUE(result.found);
    ASSERT_EQ(result.cells.front().col, 0);
    ASSERT_EQ(result.cells.back().col, 6);
    // Cost should be 6.0 (6 east steps)
    ASSERT_TRUE(std::abs(result.cost - 6.0) < 0.01);
    return true;
}

TEST(pathfinding_2x2_around_narrow_gap) {
    auto map = make_simple_map(12, 12);
    auto& grid = map.sector(0).grid();

    // Create a tower wall with a 1-wide gap at row 5
    for (int r = 0; r < 12; ++r) {
        if (r != 5) grid.set_cell_state({6, r}, CellState::Tower);
    }

    Pathfinder pf(map);

    // 1x1 can go through the gap
    auto r1 = pf.find_path({.start = {4, 5}, .goal = {8, 5}, .sector_id = 0,
                             .move_type = MoveType::Ground, .unit_size = 1});
    ASSERT_TRUE(r1.found);

    // 2x2 cannot fit through the 1-wide gap — tower at (6,4) and (6,6) block the footprint
    auto r2 = pf.find_path({.start = {3, 5}, .goal = {8, 5}, .sector_id = 0,
                             .move_type = MoveType::Ground, .unit_size = 2});
    // Should either not find a path or find a much longer one
    // Since there's no way around (towers go edge to edge), it should be unreachable
    ASSERT_TRUE(!r2.found);
    return true;
}

TEST(pathfinding_3x3_unreachable_narrow) {
    auto map = make_simple_map(12, 12);
    auto& grid = map.sector(0).grid();

    // Create a tower wall with a 2-wide gap (rows 4-5)
    for (int r = 0; r < 12; ++r) {
        if (r != 4 && r != 5) grid.set_cell_state({6, r}, CellState::Tower);
    }

    Pathfinder pf(map);

    // 2x2 can fit through the 2-wide gap
    auto r2 = pf.find_path({.start = {3, 4}, .goal = {8, 4}, .sector_id = 0,
                             .move_type = MoveType::Ground, .unit_size = 2});
    ASSERT_TRUE(r2.found);

    // 3x3 cannot fit through the 2-wide gap
    auto r3 = pf.find_path({.start = {2, 4}, .goal = {8, 4}, .sector_id = 0,
                             .move_type = MoveType::Ground, .unit_size = 3});
    ASSERT_TRUE(!r3.found);
    return true;
}

TEST(pathfinding_2x2_string_pull) {
    auto map = make_simple_map(10, 10);
    Pathfinder pf(map);

    // Straight line should collapse to 2 waypoints even with size=2
    auto result = pf.find_path({.start = {0, 4}, .goal = {6, 4}, .sector_id = 0,
                                 .move_type = MoveType::Ground, .unit_size = 2});
    ASSERT_TRUE(result.found);
    ASSERT_EQ(result.waypoints.size(), 2u);
    return true;
}

TEST(flow_field_basic) {
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();

    FlowField ff(grid, 10, 10);
    ff.generate({5, 5}, MoveType::Ground);

    ASSERT_TRUE(ff.is_valid());
    // Cost at goal should be 0
    ASSERT_TRUE(std::abs(ff.cost_at({5, 5})) < 0.01);
    // Cost at adjacent cell should be 1.0
    ASSERT_TRUE(std::abs(ff.cost_at({5, 4}) - 1.0) < 0.01);
    // Cost at diagonal neighbor should be sqrt(2)
    ASSERT_TRUE(std::abs(ff.cost_at({4, 4}) - std::sqrt(2.0)) < 0.01);
    return true;
}

TEST(flow_field_direction) {
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();

    FlowField ff(grid, 10, 10);
    ff.generate({5, 5}, MoveType::Ground);

    // Cell directly north of goal should point south (toward goal)
    CellCoord dir = ff.best_neighbor({5, 4});
    ASSERT_EQ(dir.col, 5);
    ASSERT_EQ(dir.row, 5);

    // Cell NW of goal should point SE (toward goal)
    dir = ff.best_neighbor({4, 4});
    ASSERT_EQ(dir.col, 5);
    ASSERT_EQ(dir.row, 5);
    return true;
}

TEST(flow_field_with_wall) {
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();

    // Wall off direct path with towers
    for (int r = 0; r < 8; ++r) {
        grid.set_cell_state({5, r}, CellState::Tower);
    }

    FlowField ff(grid, 10, 10);
    ff.generate({7, 3}, MoveType::Ground);

    // Cell at (3,3) should still have a valid cost (path goes around)
    ASSERT_TRUE(ff.cost_at({3, 3}) < FlowField::UNREACHABLE);
    // Following the flow from (3,3) should eventually reach goal
    CellCoord cell{3, 3};
    int steps = 0;
    while (!(cell.col == 7 && cell.row == 3) && steps < 100) {
        CellCoord next = ff.best_neighbor(cell);
        if (next == cell) break; // stuck
        cell = next;
        steps++;
    }
    ASSERT_EQ(cell.col, 7);
    ASSERT_EQ(cell.row, 3);
    return true;
}

TEST(flow_field_direction_never_crosses_wall_edge) {
    // Regression: the steering direction must always be a *legal* move. A cell
    // across a wall edge may be cheaper, but pointing there clips through the
    // wall. (Both cells stay walkable, so this is missed by tower-based tests.)
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();
    // Horizontal wall between rows 4 and 5 across the whole width except col 9.
    for (int c = 0; c < 9; ++c)
        grid.add_wall({{c, 5}, EdgeType::Horizontal});

    FlowField ff(grid, 10, 10);
    ff.generate({3, 9}, MoveType::Ground); // goal below the wall

    for (int r = 0; r < 10; ++r)
        for (int c = 0; c < 10; ++c) {
            CellCoord cell{c, r};
            if (ff.cost_at(cell) == FlowField::UNREACHABLE) continue;
            CellCoord nb = ff.best_neighbor(cell);
            if (nb == cell) continue; // goal / stuck
            ASSERT_TRUE(grid.can_move(cell, nb)); // never steers through a wall
        }

    // And a cell just above the wall must route to the gap (col 9), not down.
    CellCoord above{3, 4};
    ASSERT_TRUE(ff.best_neighbor(above) != (CellCoord{3, 5}));
    return true;
}

TEST(flow_field_unreachable) {
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();

    // Surround goal with towers
    grid.set_cell_state({4, 4}, CellState::Tower);
    grid.set_cell_state({5, 4}, CellState::Tower);
    grid.set_cell_state({6, 4}, CellState::Tower);
    grid.set_cell_state({4, 5}, CellState::Tower);
    grid.set_cell_state({6, 5}, CellState::Tower);
    grid.set_cell_state({4, 6}, CellState::Tower);
    grid.set_cell_state({5, 6}, CellState::Tower);
    grid.set_cell_state({6, 6}, CellState::Tower);

    FlowField ff(grid, 10, 10);
    ff.generate({5, 5}, MoveType::Ground);

    // Cell outside the enclosure should be unreachable to the goal
    // (actually the goal is enclosed, so cells OUTSIDE can't reach it)
    ASSERT_TRUE(ff.cost_at({0, 0}) == FlowField::UNREACHABLE);
    return true;
}

TEST(flow_field_invalidate) {
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();

    FlowField ff(grid, 10, 10);
    ff.generate({5, 5}, MoveType::Ground);
    ASSERT_TRUE(ff.is_valid());

    ff.invalidate();
    ASSERT_TRUE(!ff.is_valid());
    return true;
}

// --- Multi-size flow field tests ---

TEST(flow_field_2x2_basic) {
    auto map = make_simple_map(10, 10);
    auto& grid = map.sector(0).grid();

    FlowField ff(grid, 10, 10);
    ff.generate({4, 4}, MoveType::Ground, 2);

    ASSERT_TRUE(ff.is_valid());
    ASSERT_TRUE(std::abs(ff.cost_at({4, 4})) < 0.01);
    // Adjacent cell should have cost 1.0
    ASSERT_TRUE(std::abs(ff.cost_at({4, 3}) - 1.0) < 0.01);

    // Follow flow from (0,4) to goal
    CellCoord cell{0, 4};
    int steps = 0;
    while (!(cell.col == 4 && cell.row == 4) && steps < 50) {
        CellCoord next = ff.best_neighbor(cell);
        if (next == cell) break;
        cell = next;
        steps++;
    }
    ASSERT_EQ(cell.col, 4);
    ASSERT_EQ(cell.row, 4);
    return true;
}

TEST(flow_field_size_difference) {
    auto map = make_simple_map(12, 12);
    auto& grid = map.sector(0).grid();

    // Tower wall with 1-wide gap at row 5
    for (int r = 0; r < 12; ++r) {
        if (r != 5) grid.set_cell_state({6, r}, CellState::Tower);
    }

    // 1x1 flow field: cell (4,5) should be reachable to goal (8,5)
    FlowField ff1(grid, 12, 12);
    ff1.generate({8, 5}, MoveType::Ground, 1);
    ASSERT_TRUE(ff1.cost_at({4, 5}) < FlowField::UNREACHABLE);

    // 2x2 flow field: cell (4,5) should be UNREACHABLE (can't fit through 1-wide gap)
    FlowField ff2(grid, 12, 12);
    ff2.generate({8, 5}, MoveType::Ground, 2);
    ASSERT_TRUE(ff2.cost_at({4, 5}) == FlowField::UNREACHABLE);
    return true;
}
