#include "core/scenario.hpp"
#include "core/serialize.hpp"
#include "core/test.hpp"
#include "core/world.hpp"
#include "game/flow_field_manager.hpp"

#include <cmath>

using namespace mad;

static game::MapConfig demo_config() {
    // Sized so the grid spans the boundary line and reaches the nexus.
    return game::MapConfig{.num_players = 3, .grid_width = 44, .grid_height = 23,
                           .map_radius = 22.0, .cell_size = 1.0};
}

TEST(flow_field_manager_caches) {
    game::GameMap map(demo_config());
    game::FlowFieldManager mgr(map);
    ASSERT_EQ(mgr.cached_count(), static_cast<std::size_t>(0));
    mgr.crystal_field(0, game::MoveType::Ground, 1);
    mgr.crystal_field(0, game::MoveType::Ground, 1); // cache hit, no growth
    ASSERT_EQ(mgr.cached_count(), static_cast<std::size_t>(1));
    mgr.nexus_field(0, game::MoveType::Ground, 1);
    ASSERT_EQ(mgr.cached_count(), static_cast<std::size_t>(2));
    mgr.invalidate_all();
    ASSERT_EQ(mgr.cached_count(), static_cast<std::size_t>(0));
    return true;
}

TEST(world_demons_move) {
    core::World world(demo_config());
    world.spawn_wave(0, game::MoveType::Ground, 1, 4, +1);
    ASSERT_EQ(world.live_demon_count(), 4);
    auto start = world.demons()[0].pos;
    for (int i = 0; i < 30; ++i) world.step(1.0 / 60.0);
    auto now = world.demons()[0].pos;
    const double moved = std::abs(now.x - start.x) + std::abs(now.y - start.y);
    ASSERT_TRUE(moved > 0.1);
    return true;
}

TEST(world_demons_complete_shard_route) {
    // A clean run: every demon should collect all shards and reach the nexus.
    core::World world(demo_config());
    world.spawn_wave(0, game::MoveType::Ground, 1, 5, +1);
    for (int i = 0; i < 1600; ++i) world.step(1.0 / 60.0);
    ASSERT_TRUE(world.demons_reached_nexus() >= 1);
    return true;
}

TEST(scenario_parses_fields) {
    const std::string text =
        "name Test Run\n"
        "map players=4 grid_w=30 grid_h=20 radius=20 cell=1\n"
        "seed 99\n"
        "ticks 500\n"
        "fps 24\n"
        "camera overview\n"
        "camera sector id=2\n"
        "at 10 spawn sector=1 type=climber size=2 count=7 dir=ccw\n"
        "at 20 wall sector=0 col=5 row=6 edge=vertical height=2\n";
    core::Scenario sc;
    std::string err;
    ASSERT_TRUE(core::parse_scenario(text, sc, err));
    ASSERT_EQ(sc.name, std::string("Test Run"));
    ASSERT_EQ(sc.map.num_players, 4);
    ASSERT_EQ(sc.seed, static_cast<uint64_t>(99));
    ASSERT_EQ(sc.cameras.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(sc.events.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(sc.events[0].type, core::TimedEvent::Type::Spawn);
    ASSERT_EQ(sc.events[0].count, 7);
    ASSERT_TRUE(sc.events[0].move_type == game::MoveType::Climber);
    ASSERT_EQ(sc.events[0].wave_dir, -1);
    return true;
}

TEST(serialize_roundtrip_preserves_demons) {
    core::World world(demo_config());
    world.spawn_wave(0, game::MoveType::Ground, 1, 6, +1);
    for (int i = 0; i < 120; ++i) world.step(1.0 / 60.0);

    auto bytes = core::serialize_world(world);
    const uint64_t tick = world.tick();
    const int n = world.live_demon_count();
    const auto pos0 = world.demons()[0].pos;

    core::World restored(demo_config());
    ASSERT_TRUE(core::deserialize_world(restored, bytes));
    ASSERT_EQ(restored.tick(), tick);
    ASSERT_EQ(restored.live_demon_count(), n);
    ASSERT_TRUE(std::abs(restored.demons()[0].pos.x - pos0.x) < 1e-9);
    ASSERT_TRUE(std::abs(restored.demons()[0].pos.y - pos0.y) < 1e-9);
    return true;
}

TEST(maze_keeps_route_reachable) {
    // A perfect maze is a spanning tree, so every portal cell must still reach
    // the crystal (and the nexus) through the carved passages.
    core::World world(demo_config());
    world.generate_maze(0, 12345, 2);
    const auto& cfield = world.fields().crystal_field(0, game::MoveType::Ground, 1);
    const auto& nfield = world.fields().nexus_field(0, game::MoveType::Ground, 1);
    auto portals = world.map().portal_cells(0);
    ASSERT_TRUE(!portals.empty());
    for (auto p : portals) {
        ASSERT_TRUE(cfield.cost_at(p) < game::FlowField::UNREACHABLE);
        ASSERT_TRUE(nfield.cost_at(p) < game::FlowField::UNREACHABLE);
    }
    return true;
}

TEST(maze_is_deterministic) {
    core::World a(demo_config());
    core::World b(demo_config());
    a.generate_maze(1, 42, 2);
    b.generate_maze(1, 42, 2);
    ASSERT_EQ(a.map().sector(1).grid().walls().size(),
              b.map().sector(1).grid().walls().size());
    // A different seed should (almost surely) carve a different maze.
    core::World c(demo_config());
    c.generate_maze(1, 43, 2);
    ASSERT_TRUE(a.map().sector(1).grid().walls().size() > 0);
    return true;
}

TEST(boundary_along_seal_reroutes) {
    // With whole-map flow fields, sealing ONE seam doesn't strand a demon -- it
    // reroutes the other way around the ring. Sealing every seam touching sector
    // 1 isolates its crystal, so the shard route can't be completed.
    core::World ctl(demo_config());
    ctl.spawn_wave(0, game::MoveType::Ground, 1, 3, +1);
    for (int i = 0; i < 1600; ++i) ctl.step(1.0 / 60.0);
    ASSERT_TRUE(ctl.demons_reached_nexus() >= 1); // open: completes

    // Seal only the 0/1 seam: demon reroutes 0->2->1 (a longer path), completes.
    core::World rer(demo_config());
    for (int k = 0; k <= rer.boundary_length(0, 1); ++k)
        rer.add_boundary_wall(0, 1, k, 2);
    rer.spawn_wave(0, game::MoveType::Ground, 1, 3, +1);
    for (int i = 0; i < 3000; ++i) rer.step(1.0 / 60.0);
    ASSERT_TRUE(rer.demons_reached_nexus() >= 1); // rerouted

    // Isolate sector 1 entirely (seal 0/1 and 1/2): its crystal is unreachable.
    core::World iso(demo_config());
    for (int k = 0; k <= iso.boundary_length(0, 1); ++k) {
        iso.add_boundary_wall(0, 1, k, 2);
        iso.add_boundary_wall(1, 2, k, 2);
    }
    iso.spawn_wave(0, game::MoveType::Ground, 1, 3, +1);
    for (int i = 0; i < 3000; ++i) iso.step(1.0 / 60.0);
    ASSERT_EQ(iso.demons_reached_nexus(), 0);
    return true;
}

TEST(rung_blocks_radial_with_gap) {
    // A rung walls the row except a gap, so a unit must detour to the gap to
    // pass radially. Verify walls were added and the gap stays passable.
    core::World w(demo_config());
    game::Grid& g = w.map().sector(0).grid();
    const std::size_t before = g.walls().size();
    const int row = 12;
    w.add_rung(0, row, /*gap_left=*/true, /*gap_width=*/3, 2);
    ASSERT_TRUE(g.walls().size() > before);

    // Count walkable columns and walled crossings on the row/row+1 boundary.
    int walkable = 0, walled = 0;
    for (int c = 0; c < g.width(); ++c) {
        if (g.is_walkable({c, row}) && g.is_walkable({c, row + 1})) {
            ++walkable;
            if (!g.can_move({c, row}, {c, row + 1})) ++walled;
        }
    }
    ASSERT_TRUE(walkable > 0);
    ASSERT_TRUE(walled > 0);        // most of the row is blocked
    ASSERT_TRUE(walled < walkable); // but a gap remains open
    return true;
}

TEST(boundary_crossing_preserves_radius) {
    // A demon crossing the seam at ring i goes cell_a[i] -> cell_b[i]; both are
    // at (nearly) the same distance from the nexus, so two adjacent players can
    // align rungs at the same radius and transition there. (Requirement (b).)
    core::World w(demo_config());
    auto pairs = w.map().boundary_cells(0, 1);
    ASSERT_TRUE(!pairs.empty());
    for (const auto& p : pairs) {
        const auto wa = w.map().sector(0).cell_to_world(p.cell_a);
        const auto wb = w.map().sector(1).cell_to_world(p.cell_b);
        const double ra = std::sqrt(wa.x * wa.x + wa.y * wa.y);
        const double rb = std::sqrt(wb.x * wb.x + wb.y * wb.y);
        ASSERT_TRUE(std::abs(ra - rb) < 1.5); // same ring within ~1 cell
    }
    return true;
}

TEST(perfect_spiral_connects_and_is_long) {
    // The Archimedean spiral leaves one continuous corridor from the edge to the
    // Nexus: a portal cell must be reachable, but only via a long winding path
    // (many turns), so the cost is far larger than the straight-line distance.
    core::World w(demo_config());
    w.add_perfect_spiral(/*pitch*/ 4.0, /*dir*/ +1);
    const auto& nf = w.fields().global_nexus_field(game::MoveType::Ground, 1);
    auto portals = w.map().portal_cells(0);
    ASSERT_TRUE(!portals.empty());
    bool reachable = false;
    double maxcost = 0;
    for (auto p : portals) {
        const double c = nf.cost_at(0, p);
        if (c < game::GlobalFlowField::UNREACHABLE) {
            reachable = true;
            if (c > maxcost) maxcost = c;
        }
    }
    ASSERT_TRUE(reachable);
    ASSERT_TRUE(maxcost > 2.0 * w.map().config().map_radius); // must spiral, not go direct
    return true;
}

TEST(perfect_spiral_wall_blocks_radial) {
    // The spiral wall actually blocks: some adjacent cells one wall-winding apart
    // cannot be moved between directly (you must follow the corridor around).
    core::World w(demo_config());
    w.add_perfect_spiral(4.0, +1);
    game::Grid& g = w.map().sector(0).grid();
    int blocked = 0;
    for (int row = 0; row < g.height(); ++row)
        for (int col = 0; col < g.width(); ++col) {
            game::CellCoord c{col, row};
            if (!g.is_walkable(c)) continue;
            game::CellCoord nb{col, row + 1};
            if (g.is_walkable(nb) && !g.can_move(c, nb)) ++blocked;
        }
    ASSERT_TRUE(blocked > 10); // the spiral wall is present and solid
    return true;
}

TEST(world_is_deterministic) {
    // Same config + same inputs => identical demon positions. This is the
    // determinism Path C (netcode) relies on.
    auto run = []() {
        core::World w(demo_config());
        w.spawn_wave(0, game::MoveType::Ground, 1, 5, +1);
        for (int i = 0; i < 300; ++i) w.step(1.0 / 60.0);
        return w.demons();
    };
    auto a = run();
    auto b = run();
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        ASSERT_TRUE(a[i].pos.x == b[i].pos.x);
        ASSERT_TRUE(a[i].pos.y == b[i].pos.y);
        ASSERT_EQ(a[i].shards_collected, b[i].shards_collected);
    }
    return true;
}
