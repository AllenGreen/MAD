#include "core/coords_dump.hpp"

#include "game/game_map.hpp"

#include <cmath>
#include <cstdio>

namespace mad::core {

// Emits the exact sector/seam geometry the engine uses, so a documentation page
// can render diagrams that provably match the code. Line-based text (easy to
// parse), all coordinates in world units (origin = nexus, +Y up).
bool dump_coords(const std::string& path) {
    using namespace mad::game;

    // Same map the demos/tests use.
    MapConfig cfg{.num_players = 3, .grid_width = 44, .grid_height = 23,
                  .map_radius = 22.0, .cell_size = 1.0};
    GameMap map(cfg);

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    std::fprintf(f, "config players=%d grid_w=%d grid_h=%d radius=%g cell=%g\n",
                 cfg.num_players, cfg.grid_width, cfg.grid_height, cfg.map_radius,
                 cfg.cell_size);

    const int A = 0, B = 1;
    const double rotA = map.sector(A).rotation();
    const double rotB = map.sector(B).rotation();
    double diff = rotB - rotA;
    while (diff > M_PI) diff -= 2.0 * M_PI;
    while (diff < -M_PI) diff += 2.0 * M_PI;
    const double phi = rotA + diff * 0.5; // boundary (seam) angle
    std::fprintf(f, "seam a=%d b=%d angle_deg=%g rotA_deg=%g rotB_deg=%g\n", A, B,
                 phi * 180.0 / M_PI, rotA * 180.0 / M_PI, rotB * 180.0 / M_PI);

    const double sn = std::sin(phi), cs = std::cos(phi);
    const double px = cs, py = -sn; // perpendicular to the radial seam

    struct View { const char* label; double radius; };
    const View views[] = {{"Near the Nexus", 3.0},
                          {"Half-way out", 11.0},
                          {"At the extreme radius", 20.0}};

    const double window = 4.6; // world-unit half-window around the seam point

    for (const View& v : views) {
        const double d = v.radius;
        const double Px = d * sn, Py = d * cs;
        std::fprintf(f, "view label=%s radius=%g seampoint=%g,%g\n", v.label, d, Px, Py);

        for (int sect : {A, B}) {
            const Sector& sec = map.sector(sect);
            const Grid& g = sec.grid();
            for (int row = 0; row < g.height(); ++row)
                for (int col = 0; col < g.width(); ++col) {
                    const CellCoord c{col, row};
                    const WorldPos w = sec.cell_to_world(c);
                    if (std::hypot(w.x - Px, w.y - Py) > window) continue;
                    const bool walk = g.is_walkable(c);
                    WorldPos cn[4]; // polar corners: outer-L, outer-R, inner-R, inner-L
                    sec.cell_corners(c, cn);
                    std::fprintf(f,
                        "cell sect=%d col=%d row=%d cx=%g cy=%g r=%g walk=%d "
                        "corners=%g,%g;%g,%g;%g,%g;%g,%g\n",
                        sect, col, row, w.x, w.y, std::hypot(w.x, w.y), walk ? 1 : 0,
                        cn[0].x, cn[0].y, cn[1].x, cn[1].y, cn[2].x, cn[2].y, cn[3].x, cn[3].y);
                }
        }

        // The exact crossing pairing (global_flow_field.cpp): nudge the seam point
        // 0.7 perpendicular into each sector, then world_to_cell on each side.
        const WorldPos plus{Px + 0.7 * px, Py + 0.7 * py};
        const WorldPos minus{Px - 0.7 * px, Py - 0.7 * py};
        const bool plusInA = map.sector(A).contains_world(plus);
        const WorldPos pa = plusInA ? plus : minus;
        const WorldPos pb = plusInA ? minus : plus;
        const CellCoord ca = map.sector(A).world_to_cell(pa);
        const CellCoord cb = map.sector(B).world_to_cell(pb);
        const WorldPos cca = map.sector(A).cell_to_world(ca);
        const WorldPos ccb = map.sector(B).cell_to_world(cb);
        std::fprintf(f,
            "cross sample=%g,%g nudgeA=%g,%g nudgeB=%g,%g cellA=%d,%d cellB=%d,%d "
            "centerA=%g,%g centerB=%g,%g\n",
            Px, Py, pa.x, pa.y, pb.x, pb.y, ca.col, ca.row, cb.col, cb.row, cca.x,
            cca.y, ccb.x, ccb.y);
    }

    std::fclose(f);
    return true;
}

} // namespace mad::core
