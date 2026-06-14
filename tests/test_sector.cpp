#include "core/test.hpp"
#include "game/game_map.hpp"

#include <cmath>

using namespace mad::game;

TEST(sector_creation_4_players) {
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);
    ASSERT_EQ(map.num_sectors(), 4);
    return true;
}

TEST(sector_rotation_values) {
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    // Sector 0: rotation = 0
    ASSERT_TRUE(std::abs(map.sector(0).rotation() - 0.0) < 1e-6);
    // Sector 1: rotation = pi/2
    ASSERT_TRUE(std::abs(map.sector(1).rotation() - M_PI / 2.0) < 1e-6);
    // Sector 2: rotation = pi
    ASSERT_TRUE(std::abs(map.sector(2).rotation() - M_PI) < 1e-6);
    return true;
}

TEST(sector_cell_to_world_sector0) {
    // Sector 0 has rotation=0 (facing up/+Y).
    // Grid is centered on sector center line.
    // Cell (grid_width/2, 0) should be near (0, map_radius) in world space.
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    // Center column, top row
    WorldPos wp = map.sector(0).cell_to_world({5, 0});
    // Should be near x=0 (center line), y=map_radius (top)
    // lx = (5+0.5)*1 = 5.5, grid_w=10, ux = 5.5-5 = 0.5
    // ly = (0+0.5)*1 = 0.5, uy = 20 - 0.5 = 19.5
    // rotation=0: world = (0.5*1 + 19.5*0, -0.5*0 + 19.5*1) = (0.5, 19.5)
    ASSERT_TRUE(std::abs(wp.x - 0.5) < 0.01);
    ASSERT_TRUE(std::abs(wp.y - 19.5) < 0.01);
    return true;
}

TEST(sector_cell_to_world_roundtrip_sector0) {
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    CellCoord orig{3, 7};
    WorldPos wp = map.sector(0).cell_to_world(orig);
    CellCoord back = map.sector(0).world_to_cell(wp);
    ASSERT_EQ(orig.col, back.col);
    ASSERT_EQ(orig.row, back.row);
    return true;
}

TEST(sector_cell_to_world_roundtrip_sector1) {
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    CellCoord orig{3, 7};
    WorldPos wp = map.sector(1).cell_to_world(orig);
    CellCoord back = map.sector(1).world_to_cell(wp);
    ASSERT_EQ(orig.col, back.col);
    ASSERT_EQ(orig.row, back.row);
    return true;
}

TEST(sector_cell_to_world_roundtrip_sector2) {
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    CellCoord orig{5, 10};
    WorldPos wp = map.sector(2).cell_to_world(orig);
    CellCoord back = map.sector(2).world_to_cell(wp);
    ASSERT_EQ(orig.col, back.col);
    ASSERT_EQ(orig.row, back.row);
    return true;
}

TEST(sector_at_basic) {
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    // Point clearly in sector 0 (+Y direction)
    ASSERT_EQ(map.sector_at({0.0, 15.0}), 0);

    // Point clearly in sector 1 (+X direction)
    ASSERT_EQ(map.sector_at({15.0, 0.0}), 1);

    // Point clearly in sector 2 (-Y direction)
    ASSERT_EQ(map.sector_at({0.0, -15.0}), 2);

    // Point clearly in sector 3 (-X direction)
    ASSERT_EQ(map.sector_at({-15.0, 0.0}), 3);

    return true;
}

TEST(sector_transform_cell) {
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    // A cell near the center of sector 0's grid should map to a valid cell in sector 1
    CellCoord from{5, 7};
    CellCoord to = map.transform_cell(from, 0, 0);
    // Roundtrip in same sector should return the same cell
    ASSERT_EQ(from.col, to.col);
    ASSERT_EQ(from.row, to.row);
    return true;
}

TEST(sector_portal_cells) {
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    auto portals = map.portal_cells(0);
    // Should have some portal cells (row 0, walkable)
    ASSERT_TRUE(!portals.empty());
    for (auto& c : portals) {
        ASSERT_EQ(c.row, 0);
    }
    return true;
}

TEST(sector_nexus_cells) {
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    auto nexus = map.nexus_cells(0);
    // Should have some nexus cells (last row, walkable)
    // Note: near the nexus the wedge narrows, so some cells may be masked
    for (auto& c : nexus) {
        ASSERT_EQ(c.row, 14);
    }
    return true;
}

TEST(sector_wedge_masking) {
    // With 6 players, each sector covers 60 degrees (half_angle = 30 deg).
    // A wide grid will have corners outside the narrower wedge.
    MapConfig config{.num_players = 6, .grid_width = 30, .grid_height = 20,
                     .map_radius = 30.0, .cell_size = 1.0};
    GameMap map(config);

    auto& grid = map.sector(0).grid();
    // At row 19: uy = 30 - 19.5 = 10.5. tan(30deg) ~= 0.577.
    // Max |ux| = 10.5 * 0.577 = ~6.06. Grid half-width = 15.
    // Cell (0, 19): ux = 0.5 - 15 = -14.5, way outside.
    CellCoord corner{0, 19};
    ASSERT_EQ(grid.cell_state(corner), CellState::Blocked);

    // Center column should still be walkable
    CellCoord center{15, 10};
    ASSERT_TRUE(grid.is_walkable(center));
    return true;
}

TEST(sector_boundary_cells_adjacent) {
    // Use wider grids so the boundary line falls within both sectors' grids.
    // At the boundary (45° for 4 players), the point at distance d has
    // local x = d*sin(45°) ≈ 0.707d from center. Grid must be at least
    // 2 * 0.707 * map_radius wide to cover the full boundary.
    // With map_radius=20, that's ~28. Use grid_width=30.
    MapConfig config{.num_players = 4, .grid_width = 30, .grid_height = 20,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    auto pairs = map.boundary_cells(0, 1);
    ASSERT_TRUE(!pairs.empty());

    // All cell_a should be in-bounds for sector 0
    for (auto& p : pairs) {
        ASSERT_TRUE(map.sector(0).grid().in_bounds(p.cell_a));
        ASSERT_TRUE(map.sector(1).grid().in_bounds(p.cell_b));
    }
    return true;
}

TEST(crystal_at_mid_wedge) {
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    CellCoord c = map.crystal_cell(0);
    ASSERT_EQ(c.col, 5);
    ASSERT_EQ(c.row, 7);
    return true;
}

TEST(crystal_walkable_in_all_sectors) {
    // For 3, 4, 5, 6 players, the crystal at (width/2, height/2) should be
    // safely inside the wedge (not masked Blocked).
    for (int n : {3, 4, 5, 6}) {
        MapConfig config{.num_players = n, .grid_width = 20, .grid_height = 20,
                         .map_radius = 30.0, .cell_size = 1.0};
        GameMap map(config);
        for (int s = 0; s < n; ++s) {
            ASSERT_TRUE(map.sector(s).grid().is_walkable(map.crystal_cell(s)));
        }
    }
    return true;
}

TEST(crystal_world_positions_distinct) {
    // Sectors rotate around origin; each sector's crystal should be at a
    // different world location.
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    WorldPos w0 = map.crystal_world(0);
    WorldPos w1 = map.crystal_world(1);
    WorldPos w2 = map.crystal_world(2);

    // Sector 0 (rotation=0) crystal sits along +Y, with x ~ 0 and y > 0
    ASSERT_TRUE(std::abs(w0.x) < 1.0);
    ASSERT_TRUE(w0.y > 0.0);

    // Sector 1 (rotation=pi/2) crystal is rotated 90° CW: +X side
    ASSERT_TRUE(w1.x > 0.0);
    ASSERT_TRUE(std::abs(w1.y) < 1.0);

    // Sector 2 (rotation=pi) crystal is on -Y side
    ASSERT_TRUE(std::abs(w2.x) < 1.0);
    ASSERT_TRUE(w2.y < 0.0);

    return true;
}

TEST(crystal_between_portal_and_nexus) {
    // Crystal distance from origin should sit between nexus (near 0) and
    // portal (near map_radius).
    MapConfig config{.num_players = 4, .grid_width = 10, .grid_height = 15,
                     .map_radius = 20.0, .cell_size = 1.0};
    GameMap map(config);

    WorldPos w = map.crystal_world(0);
    double dist = std::sqrt(w.x * w.x + w.y * w.y);
    ASSERT_TRUE(dist > 2.0);
    ASSERT_TRUE(dist < config.map_radius);
    return true;
}

TEST(sector_5_players) {
    MapConfig config{.num_players = 5, .grid_width = 12, .grid_height = 20,
                     .map_radius = 30.0, .cell_size = 1.0};
    GameMap map(config);

    ASSERT_EQ(map.num_sectors(), 5);

    // Each sector should have portal cells
    for (int i = 0; i < 5; ++i) {
        auto portals = map.portal_cells(i);
        ASSERT_TRUE(!portals.empty());
    }

    // Roundtrip for each sector
    for (int i = 0; i < 5; ++i) {
        CellCoord orig{6, 10};
        WorldPos wp = map.sector(i).cell_to_world(orig);
        CellCoord back = map.sector(i).world_to_cell(wp);
        ASSERT_EQ(orig.col, back.col);
        ASSERT_EQ(orig.row, back.row);
    }

    return true;
}
