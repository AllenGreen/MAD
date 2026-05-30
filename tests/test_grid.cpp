#include "core/test.hpp"
#include "game/grid.hpp"
#include "game/wall.hpp"

using namespace mad::game;

TEST(grid_creation) {
    Grid grid(10, 8);
    ASSERT_EQ(grid.width(), 10);
    ASSERT_EQ(grid.height(), 8);
    return true;
}

TEST(grid_default_state_is_empty) {
    Grid grid(5, 5);
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 5; ++c)
            ASSERT_EQ(grid.cell_state({c, r}), CellState::Empty);
    return true;
}

TEST(grid_set_get_cell_state) {
    Grid grid(5, 5);
    grid.set_cell_state({2, 3}, CellState::Tower);
    ASSERT_EQ(grid.cell_state({2, 3}), CellState::Tower);
    ASSERT_EQ(grid.cell_state({0, 0}), CellState::Empty);
    return true;
}

TEST(grid_in_bounds) {
    Grid grid(5, 5);
    ASSERT_TRUE(grid.in_bounds({0, 0}));
    ASSERT_TRUE(grid.in_bounds({4, 4}));
    ASSERT_TRUE(!grid.in_bounds({5, 0}));
    ASSERT_TRUE(!grid.in_bounds({0, 5}));
    ASSERT_TRUE(!grid.in_bounds({-1, 0}));
    return true;
}

TEST(grid_out_of_bounds_is_blocked) {
    Grid grid(5, 5);
    ASSERT_EQ(grid.cell_state({-1, 0}), CellState::Blocked);
    ASSERT_EQ(grid.cell_state({5, 5}), CellState::Blocked);
    return true;
}

TEST(grid_is_walkable) {
    Grid grid(5, 5);
    ASSERT_TRUE(grid.is_walkable({0, 0}));
    grid.set_cell_state({0, 0}, CellState::Tower);
    ASSERT_TRUE(!grid.is_walkable({0, 0}));
    grid.set_cell_state({0, 0}, CellState::Boundary);
    ASSERT_TRUE(grid.is_walkable({0, 0}));
    return true;
}

TEST(grid_walkable_neighbors_open) {
    Grid grid(5, 5);
    CellCoord out[8];

    // Center cell should have 8 neighbors
    int count = grid.walkable_neighbors({2, 2}, out);
    ASSERT_EQ(count, 8);

    // Corner cell should have 3 neighbors
    count = grid.walkable_neighbors({0, 0}, out);
    ASSERT_EQ(count, 3);

    // Edge cell should have 5 neighbors
    count = grid.walkable_neighbors({2, 0}, out);
    ASSERT_EQ(count, 5);

    return true;
}

TEST(grid_walkable_neighbors_blocked_cell) {
    Grid grid(5, 5);
    // Block east neighbor of (1,1)
    grid.set_cell_state({2, 1}, CellState::Tower);

    CellCoord out[8];
    int count = grid.walkable_neighbors({1, 1}, out);
    // Lost: E, NE, SE (because diagonal through two towers is also checked,
    // but only E is a tower, so just E is lost directly. NE and SE may still be reachable.)
    // Actually: (2,1) is Tower. Moving diag NE to (2,0): corner cells are (1,0) and (2,1).
    // (2,1) is Tower but (1,0) is Empty, so diagonal is NOT blocked by the two-tower rule.
    // So we lose only E. Count = 7.
    ASSERT_EQ(count, 7);
    return true;
}

TEST(grid_diagonal_blocked_by_two_towers) {
    Grid grid(5, 5);
    // Place towers at (2,1) and (1,0) — the two corner cells for diagonal (1,1)->(2,0)
    grid.set_cell_state({2, 1}, CellState::Tower);
    grid.set_cell_state({1, 0}, CellState::Tower);

    ASSERT_TRUE(!grid.can_move({1, 1}, {2, 0})); // NE blocked by two corner towers
    ASSERT_TRUE(grid.can_move({1, 1}, {0, 0}));  // NW still fine (corners are (0,1) and (1,0))
    // Actually (1,0) is a tower and (0,1) is empty, so NW is not blocked by two-tower rule.
    return true;
}

TEST(grid_can_move_cardinal) {
    Grid grid(5, 5);
    ASSERT_TRUE(grid.can_move({1, 1}, {2, 1}));  // E
    ASSERT_TRUE(grid.can_move({1, 1}, {0, 1}));  // W
    ASSERT_TRUE(grid.can_move({1, 1}, {1, 0}));  // N
    ASSERT_TRUE(grid.can_move({1, 1}, {1, 2}));  // S
    return true;
}

TEST(grid_can_move_not_adjacent) {
    Grid grid(5, 5);
    ASSERT_TRUE(!grid.can_move({0, 0}, {2, 0}));  // too far
    ASSERT_TRUE(!grid.can_move({0, 0}, {0, 0}));  // same cell
    return true;
}

TEST(grid_wall_blocks_cardinal) {
    Grid grid(5, 5);
    // Place horizontal wall on top of (2,2) — blocks north movement
    grid.add_wall({{2, 2}, EdgeType::Horizontal});

    ASSERT_TRUE(!grid.can_move({2, 2}, {2, 1})); // N blocked
    ASSERT_TRUE(grid.can_move({2, 2}, {2, 3}));  // S not blocked
    ASSERT_TRUE(grid.can_move({2, 2}, {1, 2}));  // W not blocked
    ASSERT_TRUE(grid.can_move({2, 2}, {3, 2}));  // E not blocked
    return true;
}

TEST(grid_wall_blocks_vertical) {
    Grid grid(5, 5);
    // Place vertical wall on left of (2,2) — blocks west movement
    grid.add_wall({{2, 2}, EdgeType::Vertical});

    ASSERT_TRUE(!grid.can_move({2, 2}, {1, 2})); // W blocked
    ASSERT_TRUE(grid.can_move({2, 2}, {3, 2}));  // E not blocked
    return true;
}

TEST(grid_diagonal_wall_blocks) {
    Grid grid(5, 5);
    // DiagNE wall at (2,2) blocks movement from (2,2) to (3,1)
    grid.add_wall({{2, 2}, EdgeType::DiagNE});

    ASSERT_TRUE(!grid.can_move({2, 2}, {3, 1})); // NE blocked by diagonal wall
    ASSERT_TRUE(grid.can_move({2, 2}, {1, 1}));  // NW not blocked
    return true;
}

TEST(grid_corner_cutting_wall_blocks_diagonal) {
    Grid grid(5, 5);
    // Place L-shaped walls: north wall + east wall at (2,2)
    // This should block NE diagonal movement
    grid.add_wall({{2, 2}, EdgeType::Horizontal}); // north wall
    grid.add_wall({{3, 2}, EdgeType::Vertical});    // east wall (left side of col 3)

    ASSERT_TRUE(!grid.can_move({2, 2}, {3, 1})); // NE blocked by corner-cutting rule
    ASSERT_TRUE(!grid.can_move({2, 2}, {2, 1})); // N blocked by horizontal wall
    return true;
}

TEST(grid_wall_add_remove) {
    Grid grid(5, 5);
    EdgeCoord edge = {{2, 2}, EdgeType::Horizontal};

    ASSERT_TRUE(!grid.has_wall(edge));
    grid.add_wall(edge);
    ASSERT_TRUE(grid.has_wall(edge));
    grid.remove_wall(edge);
    ASSERT_TRUE(!grid.has_wall(edge));
    return true;
}

// --- Multi-size unit tests ---

TEST(grid_footprint_walkable_clear) {
    Grid grid(10, 10);
    ASSERT_TRUE(grid.is_footprint_walkable({2, 2}, 1));
    ASSERT_TRUE(grid.is_footprint_walkable({2, 2}, 2));
    ASSERT_TRUE(grid.is_footprint_walkable({2, 2}, 3));
    return true;
}

TEST(grid_footprint_walkable_blocked) {
    Grid grid(10, 10);
    grid.set_cell_state({3, 3}, CellState::Tower);
    // 2x2 at (2,2) covers (2,2),(3,2),(2,3),(3,3) — tower at (3,3) blocks it
    ASSERT_TRUE(!grid.is_footprint_walkable({2, 2}, 2));
    // 2x2 at (0,0) covers (0,0),(1,0),(0,1),(1,1) — all clear
    ASSERT_TRUE(grid.is_footprint_walkable({0, 0}, 2));
    return true;
}

TEST(grid_footprint_walkable_edge_of_map) {
    Grid grid(10, 10);
    // 2x2 at (9,9) extends to (10,10) — out of bounds
    ASSERT_TRUE(!grid.is_footprint_walkable({9, 9}, 2));
    // 2x2 at (8,8) covers (8,8),(9,8),(8,9),(9,9) — just fits
    ASSERT_TRUE(grid.is_footprint_walkable({8, 8}, 2));
    return true;
}

TEST(grid_can_move_2x2_open) {
    Grid grid(10, 10);
    ASSERT_TRUE(grid.can_move({3, 3}, {4, 3}, 2));  // east
    ASSERT_TRUE(grid.can_move({3, 3}, {3, 4}, 2));  // south
    ASSERT_TRUE(grid.can_move({3, 3}, {4, 4}, 2));  // SE diagonal
    ASSERT_TRUE(grid.can_move({3, 3}, {2, 2}, 2));  // NW diagonal
    return true;
}

TEST(grid_can_move_2x2_blocked_by_tower) {
    Grid grid(10, 10);
    // Place tower at (5,3). 2x2 moving east from (3,3) to (4,3):
    // destination footprint is (4,3),(5,3),(4,4),(5,4) — tower at (5,3) blocks it
    grid.set_cell_state({5, 3}, CellState::Tower);
    ASSERT_TRUE(!grid.can_move({3, 3}, {4, 3}, 2));
    // Moving south from (3,3) to (3,4) should still work
    ASSERT_TRUE(grid.can_move({3, 3}, {3, 4}, 2));
    return true;
}

TEST(grid_can_move_2x2_narrow_gap) {
    Grid grid(10, 10);
    // Create a wall of towers with a 1-wide gap at col 5
    // Towers at col 5, rows 0-3 and 5-9 (gap at row 4)
    for (int r = 0; r < 10; ++r) {
        if (r != 4) grid.set_cell_state({5, r}, CellState::Tower);
    }
    // 1x1 can go through the gap
    ASSERT_TRUE(grid.can_move({4, 4}, {5, 4}));  // size 1 passes
    // But the gap is only 1 wide, so 2x2 can't fit (destination (5,4) includes (6,4) which is fine,
    // but also (5,5) which is a tower)
    ASSERT_TRUE(!grid.can_move({4, 4}, {5, 4}, 2));
    return true;
}

TEST(grid_wall_blocks_2x2_cardinal) {
    Grid grid(10, 10);
    // Place vertical walls blocking east movement for a 2x2 at (3,3)
    // The 2x2 moving east from (3,3) to (4,3) checks vertical edges at (5,3) and (5,4)
    grid.add_wall({{5, 3}, EdgeType::Vertical});
    ASSERT_TRUE(!grid.can_move({3, 3}, {4, 3}, 2)); // blocked by wall at (5,3)

    grid.remove_wall({{5, 3}, EdgeType::Vertical});
    ASSERT_TRUE(grid.can_move({3, 3}, {4, 3}, 2)); // now clear
    return true;
}

TEST(grid_can_move_3x3_open) {
    Grid grid(10, 10);
    ASSERT_TRUE(grid.can_move({2, 2}, {3, 2}, 3));  // east
    ASSERT_TRUE(grid.can_move({2, 2}, {3, 3}, 3));  // SE diagonal
    // Can't fit 3x3 at edge
    ASSERT_TRUE(!grid.can_move({7, 7}, {8, 7}, 3)); // destination (8,7) footprint extends to (10,9)
    return true;
}
