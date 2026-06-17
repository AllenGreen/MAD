#include "core/world.hpp"
#include "core/log.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

namespace mad::core {

using game::CellCoord;
using game::FlowField;
using game::MoveType;
using game::WorldPos;

static constexpr const char* TAG = "World";

static double dist(WorldPos a, WorldPos b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

World::World(const game::MapConfig& config, uint64_t seed)
    : map_(config), fields_(map_), rng_(seed) {
    log::info(TAG, "World created: {} sectors, grid {}x{}",
              map_.num_sectors(), config.grid_width, config.grid_height);
}

int World::live_demon_count() const {
    return static_cast<int>(std::count_if(
        demons_.begin(), demons_.end(), [](const Demon& d) { return d.alive; }));
}

int World::target_sector_for(const Demon& d) const {
    const int n = map_.num_sectors();
    // k-th crystal (k = shards_collected) sits this many sectors around the ring.
    const int off = ((d.wave_dir * d.shards_collected) % n + n) % n;
    return (d.spawn_sector + off) % n;
}

void World::spawn_wave(int sector, MoveType mt, int size, int count, int wave_dir,
                       bool nexus_goal) {
    const game::Sector& s = map_.sector(sector);
    auto portals = map_.portal_cells(sector);
    if (portals.empty()) {
        log::warn(TAG, "sector {} has no walkable portal cells", sector);
        return;
    }
    const double speed = mt == MoveType::Flyer ? 5.5
                       : mt == MoveType::Climber ? 4.5
                       : mt == MoveType::Smasher ? 3.5
                                                 : 4.0;
    const double hp = 10.0 * size * size;
    for (int i = 0; i < count; ++i) {
        Demon d;
        d.id = next_demon_id_++;
        d.move_type = mt;
        d.size = size;
        d.speed = speed;
        d.hp = d.max_hp = hp;
        d.spawn_sector = sector;
        d.sector = sector;
        d.wave_dir = wave_dir >= 0 ? +1 : -1;
        if (nexus_goal) d.shards_collected = map_.num_sectors(); // straight to Nexus
        // Spread demons across the portal edge, nudged slightly inward.
        const CellCoord cell = portals[(i * 2) % portals.size()];
        d.pos = s.cell_to_world(cell);
        demons_.push_back(d);
    }
    if (nexus_goal && count > 0 && sector == 0) {
        const auto& nf = fields_.global_nexus_field(mt, size);
        const int w = map_.config().grid_width, h = map_.config().grid_height;
        for (int row = 0; row < h; ++row) {
            std::string line;
            for (int sc = 0; sc < map_.num_sectors(); ++sc) {
                line += std::format("  s{}[", sc);
                for (int col = w / 2 - 6; col <= w / 2 + 6; col += 3) {
                    const double c = nf.cost_at(sc, {col, row});
                    line += (c >= 1e29) ? "  ." : std::format(" {:2.0f}", c);
                }
                line += "]";
            }
            log::debug(TAG, "r{:2d}{}", row, line);
        }
    }
    log::info(TAG, "spawned {} {}-size demons at sector {} (dir {})", count, size,
              sector, wave_dir >= 0 ? "CW" : "CCW");
}

void World::place_wall(int sector, game::EdgeCoord edge) {
    map_.sector(sector).grid().add_wall(edge);
    fields_.invalidate_all();
}

void World::place_tower(int sector, CellCoord origin, int size) {
    game::Grid& g = map_.sector(sector).grid();
    for (int dr = 0; dr < size; ++dr)
        for (int dc = 0; dc < size; ++dc) {
            CellCoord c{origin.col + dc, origin.row + dr};
            if (g.in_bounds(c))
                g.set_cell_state(c, game::CellState::Tower);
        }
    fields_.invalidate_all();
}

namespace {
// The wall edge sitting between two cardinally-adjacent cells.
game::EdgeCoord edge_between(CellCoord a, CellCoord b) {
    if (b.col == a.col + 1) return {{a.col + 1, a.row}, game::EdgeType::Vertical};
    if (b.col == a.col - 1) return {{a.col, a.row}, game::EdgeType::Vertical};
    if (b.row == a.row + 1) return {{a.col, a.row + 1}, game::EdgeType::Horizontal};
    return {{a.col, a.row}, game::EdgeType::Horizontal}; // b.row == a.row - 1
}

// Wall the edge between two adjacent cells (cardinal or diagonal).
void wall_between(game::Grid& g, CellCoord a, CellCoord b, game::WallData wd) {
    const int dc = b.col - a.col, dr = b.row - a.row;
    if (std::abs(dc) + std::abs(dr) == 1) {
        g.walls().add(edge_between(a, b), wd);
    } else if (std::abs(dc) == 1 && std::abs(dr) == 1) {
        if (dr < 0) g.walls().add({a, dc > 0 ? game::EdgeType::DiagNE : game::EdgeType::DiagNW}, wd);
        else        g.walls().add({b, dc > 0 ? game::EdgeType::DiagNW : game::EdgeType::DiagNE}, wd);
    }
}
} // namespace

void World::generate_maze(int sector, uint64_t seed, uint8_t wall_height) {
    game::Grid& g = map_.sector(sector).grid();
    const int w = g.width(), h = g.height();
    auto walkable = [&](CellCoord c) { return g.in_bounds(c) && g.is_walkable(c); };

    // 1. Wall every interior edge between two walkable cardinal neighbors.
    const game::WallData wd{wall_height, 255};
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c) {
            CellCoord cur{c, r};
            if (!walkable(cur)) continue;
            if (walkable({c + 1, r})) g.walls().add(edge_between(cur, {c + 1, r}), wd);
            if (walkable({c, r + 1})) g.walls().add(edge_between(cur, {c, r + 1}), wd);
        }

    // 2. Carve a spanning tree (a "perfect" maze): iterative DFS, removing the
    //    wall between a cell and a randomly chosen unvisited neighbor.
    std::vector<char> visited(static_cast<size_t>(w) * h, 0);
    auto idx = [&](CellCoord c) { return static_cast<size_t>(c.row) * w + c.col; };
    const CellCoord start = map_.crystal_cell(sector);
    if (!walkable(start)) { fields_.invalidate_all(); return; }

    std::mt19937_64 rng(seed);
    std::vector<CellCoord> stack{start};
    visited[idx(start)] = 1;
    const CellCoord dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!stack.empty()) {
        const CellCoord cur = stack.back();
        CellCoord cand[4];
        int n = 0;
        for (const auto& d : dirs) {
            const CellCoord nb{cur.col + d.col, cur.row + d.row};
            if (walkable(nb) && !visited[idx(nb)]) cand[n++] = nb;
        }
        if (n == 0) { stack.pop_back(); continue; }
        const CellCoord pick = cand[rng() % static_cast<unsigned>(n)];
        g.walls().remove(edge_between(cur, pick)); // carve a passage
        visited[idx(pick)] = 1;
        stack.push_back(pick);
    }
    fields_.invalidate_all();
}

const std::vector<game::BoundaryPair>& World::boundary_pairs(int a, int b) {
    const int key = a * 256 + b;
    auto it = boundary_cache_.find(key);
    if (it == boundary_cache_.end())
        it = boundary_cache_.emplace(key, map_.boundary_cells(a, b)).first;
    return it->second;
}

int World::boundary_length(int a, int b) {
    // Crossings are indexed by ring = rounded radius from the nexus.
    return static_cast<int>(std::lround(map_.config().map_radius)) + 1;
}

bool World::crossing_blocked(int a, int b, int index) const {
    for (const BoundaryWall& w : boundary_walls_) {
        const bool same = (w.a == a && w.b == b) || (w.a == b && w.b == a);
        if (same && w.index == index) return true;
    }
    return false;
}

void World::add_boundary_wall(int a, int b, int ring, uint8_t height) {
    // `ring` is the radius (in cells from the nexus) at which to seal the seam.
    if (ring < 0 || ring > boundary_length(a, b)) {
        log::warn(TAG, "boundary wall ring {} out of range for s{}/s{}", ring, a, b);
        return;
    }
    boundary_walls_.push_back({a, b, ring, height});
    // Drop this crossing from the flood, so demons route to an open radius.
    fields_.set_boundary_blocked(a, b, ring, true);
    fields_.invalidate_all();
}

void World::add_rung(int sector, int row, bool gap_left, int gap_width,
                     uint8_t height) {
    game::Grid& g = map_.sector(sector).grid();
    if (row < 0 || row + 1 >= g.height()) {
        log::warn(TAG, "rung row {} out of range for s{}", row, sector);
        return;
    }
    // Walkable columns where a wall between row and row+1 actually separates two
    // reachable cells -- these are the cells the rung spans.
    std::vector<int> cols;
    for (int c = 0; c < g.width(); ++c)
        if (g.is_walkable({c, row}) && g.is_walkable({c, row + 1}))
            cols.push_back(c);
    if (cols.empty()) return;

    const game::WallData wd{height, 255};
    const int n = static_cast<int>(cols.size());
    for (int i = 0; i < n; ++i) {
        // Leave a gap of gap_width cells at one border.
        const bool in_gap = gap_left ? (i < gap_width) : (i >= n - gap_width);
        if (in_gap) continue;
        g.walls().add({{cols[i], row + 1}, game::EdgeType::Horizontal}, wd);
    }
    fields_.invalidate_all();
}

void World::add_perfect_spiral(double pitch, int dir, uint8_t height) {
    // A single continuous Archimedean spiral. Define a scalar field
    //     f(r, theta) = (R0 - r)/pitch + dir*theta/(2*pi)
    // whose integer contours ARE the spiral wall windings (radius drops by `pitch`
    // per full turn). The corridors between windings form smooth spiral channels
    // from the outer edge to the centre. We wall every edge whose two cells fall in
    // different windings (floor(f) differs) -- cardinal AND diagonal, so the wall
    // is solid -- and we apply the same test across the seams to join the wall.
    const int n = map_.num_sectors();
    const double R = map_.config().map_radius;
    const double R0 = R - 1.5;   // outermost wall radius (open just outside it)
    const double Rin = 1.5;      // open centre inside this radius (-> Nexus)
    if (pitch < 1.0) pitch = 1.0;
    const double sgn = dir >= 0 ? 1.0 : -1.0;
    const game::WallData wd{height, 255};
    static const int dirs8[8][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1},
                                    {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};

    auto crosses = [&](const WorldPos& a, const WorldPos& b) -> bool {
        const double r1 = std::hypot(a.x, a.y), r2 = std::hypot(b.x, b.y);
        const double rc = (r1 + r2) * 0.5;
        if (rc < Rin || rc > R0) return false; // wall only exists in this band
        const double th1 = std::atan2(a.x, a.y);
        double dth = std::atan2(b.x, b.y) - th1; // continuous local angle delta
        while (dth > M_PI) dth -= 2.0 * M_PI;
        while (dth < -M_PI) dth += 2.0 * M_PI;
        const double f1 = (R0 - r1) / pitch + sgn * th1 / (2.0 * M_PI);
        const double df = (r1 - r2) / pitch + sgn * dth / (2.0 * M_PI);
        return std::floor(f1) != std::floor(f1 + df);
    };

    // 1) Wall only the CARDINAL grid edges the spiral crosses, in every sector.
    //    This is the complete inside/outside boundary (a clean staircase, no
    //    teeth); the strict no-corner-cutting rule then blocks diagonal slips too
    //    (a diagonal is blocked whenever one of its corner edges is walled).
    for (int s = 0; s < n; ++s) {
        game::Sector& sec = map_.sector(s);
        game::Grid& g = sec.grid();
        for (int row = 0; row < g.height(); ++row)
            for (int col = 0; col < g.width(); ++col) {
                const CellCoord c{col, row};
                if (!g.is_walkable(c)) continue;
                const WorldPos wc = sec.cell_to_world(c);
                for (auto& [dc, dr] : dirs8) {
                    if (dc != 0 && dr != 0) continue; // cardinal edges only
                    const CellCoord nb{col + dc, row + dr};
                    if (!g.is_walkable(nb)) continue;
                    const WorldPos wn = sec.cell_to_world(nb);
                    if (crosses(wc, wn)) wall_between(g, c, nb, wd);
                }
            }
    }

    // 2) Join the wall across the seams: where the spiral crosses a seam, seal the
    //    crossing at that radius (same nudge-pairing the flow field uses).
    for (int s = 0; s < n; ++s) {
        for (int step : {1, n - 1}) {
            const int t = (s + step) % n;
            if (t == s) continue;
            double dphi = map_.sector(t).rotation() - map_.sector(s).rotation();
            while (dphi > M_PI) dphi -= 2.0 * M_PI;
            while (dphi < -M_PI) dphi += 2.0 * M_PI;
            const double phi = map_.sector(s).rotation() + dphi * 0.5;
            const double sn = std::sin(phi), cs = std::cos(phi);
            const double px = cs, py = -sn;
            for (double d = Rin; d <= R0; d += 0.5) {
                const WorldPos P{d * sn, d * cs};
                const WorldPos plus{P.x + 0.7 * px, P.y + 0.7 * py};
                const WorldPos minus{P.x - 0.7 * px, P.y - 0.7 * py};
                const bool plusInS = map_.sector(s).contains_world(plus);
                const CellCoord ca = map_.sector(s).world_to_cell(plusInS ? plus : minus);
                const CellCoord cb = map_.sector(t).world_to_cell(plusInS ? minus : plus);
                if (!map_.sector(s).grid().is_walkable(ca)) continue;
                if (!map_.sector(t).grid().is_walkable(cb)) continue;
                if (crosses(map_.sector(s).cell_to_world(ca),
                            map_.sector(t).cell_to_world(cb)))
                    add_boundary_wall(s, t, static_cast<int>(std::lround(d)), height);
            }
        }
    }
    fields_.invalidate_all();
}

void World::step(double dt) {
    for (Demon& d : demons_) {
        if (!d.alive) continue;
        step_demon(d, dt);
    }
    // Reap demons that reached the nexus.
    demons_.erase(std::remove_if(demons_.begin(), demons_.end(),
                                 [](const Demon& d) { return !d.alive; }),
                  demons_.end());
    ++tick_;
}

void World::step_demon(Demon& d, double dt) {
    const int n = map_.num_sectors();
    // d.sector is authoritative (updated when the demon crosses a seam below);
    // we don't re-derive it from position, so a crossing commits cleanly instead
    // of oscillating at the boundary.
    const game::Sector& sec = map_.sector(d.sector);

    const bool nexus_phase = d.shards_collected >= n;
    const int target = nexus_phase ? -1 : target_sector_for(d);

    // One whole-map flow field carries the demon all the way to its current goal,
    // winding across seams as needed -- no per-sector handoff, no teleport.
    const game::GlobalFlowField& field = nexus_phase
        ? fields_.global_nexus_field(d.move_type, d.size)
        : fields_.global_crystal_field(target, d.move_type, d.size);

    CellCoord cell = sec.world_to_cell(d.pos);
    cell.col = std::clamp(cell.col, 0, sec.grid().width() - 1);
    cell.row = std::clamp(cell.row, 0, sec.grid().height() - 1);

    // The next step may be a cell in this sector or a cell just across a seam.
    // Either way we steer continuously toward its world position; crossing the
    // seam line simply flips `d.sector` (handled at the top next tick). Smooth.
    const game::GlobalFlowField::Step step = field.step_at(d.sector, cell);
    const bool stuck = step.sector == d.sector && step.cell == cell;
    if (!stuck) {
        const WorldPos waypoint = map_.sector(step.sector).cell_to_world(step.cell);
        const double dx = waypoint.x - d.pos.x;
        const double dy = waypoint.y - d.pos.y;
        const double len = std::sqrt(dx * dx + dy * dy);
        const double travel = d.speed * dt;
        if (len > 1e-6) {
            const double f = std::min(1.0, travel / len);
            d.pos.x += dx * f;
            d.pos.y += dy * f;
        }
        // If this step crosses a seam, commit to the neighbour sector. The demon
        // keeps steering toward the same world point, so it glides across the
        // boundary continuously -- no teleport, no oscillation.
        if (step.sector != d.sector)
            d.sector = step.sector;
    }

    // Shard pickup: close enough to the target crystal.
    if (!nexus_phase && d.sector == target) {
        const WorldPos crystal = map_.crystal_world(target);
        if (dist(d.pos, crystal) < 1.0 + 0.5 * d.size) {
            ++d.shards_collected;
            log::debug(TAG, "demon {} collected shard {}/{}", d.id,
                       d.shards_collected, n);
        }
    }

    // Nexus arrival ends the run.
    if (nexus_phase && dist(d.pos, WorldPos{0.0, 0.0}) < 3.0 + 0.5 * d.size) {
        d.reached_nexus = true;
        d.alive = false;
        ++reached_nexus_count_;
    }
}

} // namespace mad::core
