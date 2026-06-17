#include "game/global_flow_field.hpp"
#include "core/log.hpp"

#include <cmath>
#include <queue>
#include <unordered_map>

namespace mad::game {

static constexpr double SQRT2 = 1.4142135623730951;

void GlobalFlowField::generate(const std::vector<std::pair<int, CellCoord>>& goals,
                               MoveType mt, int size,
                               const BlockedFn& crossing_blocked) {
    n_ = map_.num_sectors();
    width_ = map_.config().grid_width;
    height_ = map_.config().grid_height;
    const int cells = width_ * height_;
    cost_.assign(n_, std::vector<double>(cells, UNREACHABLE));
    dir_.assign(n_, std::vector<Step>(cells, Step{-1, {-1, -1}}));

    // Precompute open seam crossings by sampling the shared boundary line by
    // RADIUS and nudging a sample point into each sector to land on the
    // seam-adjacent walkable cell on each side. Cells on opposite sides of the
    // seam at the same radius are paired, so units cross at (densely) any radius
    // -- enabling smooth transitions and tangential travel around a ring. A
    // crossing's "ring" (rounded radius) is what an ALONG wall seals.
    auto boundary_angle = [&](int s, int t) {
        double diff = map_.sector(t).rotation() - map_.sector(s).rotation();
        while (diff > M_PI) diff -= 2.0 * M_PI;
        while (diff < -M_PI) diff += 2.0 * M_PI;
        return map_.sector(s).rotation() + diff * 0.5;
    };
    struct CrossEdge { int t; CellCoord cell; double cost; };
    std::vector<std::unordered_map<int, std::vector<CrossEdge>>> crossings(n_);
    const double R = map_.config().map_radius;
    for (int s = 0; s < n_; ++s) {
        for (int step : {1, n_ - 1}) {
            const int t = (s + step) % n_;
            if (t == s) continue;
            const double phi = boundary_angle(s, t);
            const double sn = std::sin(phi), cs = std::cos(phi);
            const double px = cs, py = -sn; // perpendicular to the (radial) seam
            CellCoord lastA{-2, -2}, lastB{-2, -2};
            for (double d = 1.5; d <= R + 0.5; d += 0.5) {
                const WorldPos P{d * sn, d * cs};
                const WorldPos plus{P.x + 0.7 * px, P.y + 0.7 * py};
                const WorldPos minus{P.x - 0.7 * px, P.y - 0.7 * py};
                const bool plusInS = map_.sector(s).contains_world(plus);
                const WorldPos ps = plusInS ? plus : minus;
                const WorldPos pt = plusInS ? minus : plus;
                const CellCoord a = map_.sector(s).world_to_cell(ps);
                const CellCoord b = map_.sector(t).world_to_cell(pt);
                if (!map_.sector(s).grid().is_footprint_walkable(a, size)) continue;
                if (!map_.sector(t).grid().is_footprint_walkable(b, size)) continue;
                if (a == lastA && b == lastB) continue; // dedup consecutive
                lastA = a; lastB = b;
                const int ring = static_cast<int>(std::lround(d));
                if (crossing_blocked && crossing_blocked(s, t, ring) && mt != MoveType::Flyer)
                    continue; // an ALONG wall seals this radius
                const double dc = std::hypot(ps.x - pt.x, ps.y - pt.y);
                crossings[s][idx(a)].push_back({t, b, dc});
            }
        }
    }

    struct QN {
        double cost;
        int sector;
        CellCoord cell;
        bool operator>(const QN& o) const { return cost > o.cost; }
    };
    std::priority_queue<QN, std::vector<QN>, std::greater<>> pq;
    for (const auto& [gs, gc] : goals) {
        if (gs >= 0 && gs < n_ && map_.sector(gs).grid().in_bounds(gc)) {
            cost_[gs][idx(gc)] = 0.0;
            dir_[gs][idx(gc)] = Step{gs, gc};
            pq.push({0.0, gs, gc});
        }
    }

    static constexpr int dirs[8][2] = {
        {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};

    while (!pq.empty()) {
        const QN cur = pq.top();
        pq.pop();
        const int s = cur.sector;
        const CellCoord cell = cur.cell;
        if (cur.cost > cost_[s][idx(cell)]) continue;
        const Grid& g = map_.sector(s).grid();

        // Grid neighbours within the same sector.
        for (auto& [dc, dr] : dirs) {
            const CellCoord nb{cell.col + dc, cell.row + dr};
            if (!g.in_bounds(nb)) continue;
            const bool passable = mt == MoveType::Flyer
                ? g.is_footprint_walkable(nb, size)
                : g.can_move(cell, nb, size);
            if (!passable) continue;
            const double nc = cur.cost + ((dc != 0 && dr != 0) ? SQRT2 : 1.0);
            if (nc < cost_[s][idx(nb)]) {
                cost_[s][idx(nb)] = nc;
                dir_[s][idx(nb)] = Step{s, cell}; // step back toward goal
                pq.push({nc, s, nb});
            }
        }

        // Seam crossings into a neighbour sector.
        auto it = crossings[s].find(idx(cell));
        if (it != crossings[s].end()) {
            for (const CrossEdge& ce : it->second) {
                const double nc = cur.cost + ce.cost;
                if (nc < cost_[ce.t][idx(ce.cell)]) {
                    cost_[ce.t][idx(ce.cell)] = nc;
                    dir_[ce.t][idx(ce.cell)] = Step{s, cell};
                    pq.push({nc, ce.t, ce.cell});
                }
            }
        }
    }
    valid_ = true;
}

GlobalFlowField::Step GlobalFlowField::step_at(int sector, CellCoord cell) const {
    if (sector < 0 || sector >= n_ || !map_.sector(sector).grid().in_bounds(cell))
        return Step{sector, cell};
    const Step s = dir_[sector][idx(cell)];
    if (s.sector >= 0) return s; // on the basin: use the recorded goal-ward step

    // Off the basin (e.g. drifted into a masked corner near a sealed seam): steer
    // back toward the nearest reachable cell so the unit re-acquires the path
    // instead of getting stuck. Search a small neighbourhood.
    double best = cost_[sector][idx(cell)];
    Step bs{sector, cell};
    for (int dr = -2; dr <= 2; ++dr)
        for (int dc = -2; dc <= 2; ++dc) {
            const CellCoord nb{cell.col + dc, cell.row + dr};
            if (!map_.sector(sector).grid().in_bounds(nb)) continue;
            const double c = cost_[sector][idx(nb)];
            if (c < best) { best = c; bs = Step{sector, nb}; }
        }
    return bs;
}

double GlobalFlowField::cost_at(int sector, CellCoord cell) const {
    if (sector < 0 || sector >= n_ || !map_.sector(sector).grid().in_bounds(cell))
        return UNREACHABLE;
    return cost_[sector][idx(cell)];
}

} // namespace mad::game
