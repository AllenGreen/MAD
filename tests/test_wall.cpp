#include "core/test.hpp"
#include "game/wall.hpp"

using namespace mad::game;

TEST(wall_add_and_has) {
    WallSet walls;
    EdgeCoord edge = {{2, 3}, EdgeType::Horizontal};

    ASSERT_TRUE(!walls.has(edge));
    walls.add(edge);
    ASSERT_TRUE(walls.has(edge));
    return true;
}

TEST(wall_remove) {
    WallSet walls;
    EdgeCoord edge = {{2, 3}, EdgeType::Vertical};

    walls.add(edge);
    ASSERT_TRUE(walls.has(edge));
    walls.remove(edge);
    ASSERT_TRUE(!walls.has(edge));
    return true;
}

TEST(wall_get_data) {
    WallSet walls;
    EdgeCoord edge = {{1, 1}, EdgeType::Horizontal};

    walls.add(edge, {.height = 3, .hp = 100});
    auto* data = walls.get(edge);
    ASSERT_TRUE(data != nullptr);
    ASSERT_EQ(data->height, 3);
    ASSERT_EQ(data->hp, 100);
    return true;
}

TEST(wall_blocks_ground) {
    WallSet walls;
    EdgeCoord edge = {{1, 1}, EdgeType::Horizontal};

    ASSERT_TRUE(!walls.blocks_ground(edge));
    walls.add(edge, {.height = 1});
    ASSERT_TRUE(walls.blocks_ground(edge)); // any wall blocks ground
    return true;
}

TEST(wall_blocks_climber_only_tall) {
    WallSet walls;
    EdgeCoord edge = {{1, 1}, EdgeType::Horizontal};

    walls.add(edge, {.height = 1});
    ASSERT_TRUE(!walls.blocks_climber(edge)); // short wall, climber gets over

    walls.add(edge, {.height = 2});
    ASSERT_TRUE(walls.blocks_climber(edge)); // tall wall, climber blocked
    return true;
}

TEST(wall_clear) {
    WallSet walls;
    walls.add({{0, 0}, EdgeType::Horizontal});
    walls.add({{1, 1}, EdgeType::Vertical});
    ASSERT_EQ(walls.size(), 2u);
    walls.clear();
    ASSERT_EQ(walls.size(), 0u);
    return true;
}

TEST(wall_diagonal_types) {
    WallSet walls;
    EdgeCoord ne = {{2, 2}, EdgeType::DiagNE};
    EdgeCoord nw = {{2, 2}, EdgeType::DiagNW};

    walls.add(ne);
    walls.add(nw);

    ASSERT_TRUE(walls.has(ne));
    ASSERT_TRUE(walls.has(nw));
    ASSERT_EQ(walls.size(), 2u); // they are distinct edges
    return true;
}

TEST(wall_overwrite_data) {
    WallSet walls;
    EdgeCoord edge = {{1, 1}, EdgeType::Horizontal};

    walls.add(edge, {.height = 1, .hp = 100});
    walls.add(edge, {.height = 2, .hp = 50});

    auto* data = walls.get(edge);
    ASSERT_TRUE(data != nullptr);
    ASSERT_EQ(data->height, 2);
    ASSERT_EQ(data->hp, 50);
    ASSERT_EQ(walls.size(), 1u); // still one wall, just updated
    return true;
}
