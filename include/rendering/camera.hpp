#pragma once

#include "game/grid_types.hpp"

#include <cmath>

namespace mad::rendering {

// A camera projects world space (origin = Nexus, +Y up, angles measured from +Y
// clockwise) onto a pixel viewport (origin = top-left, +Y down).
//
// `angle` lets a camera rotate the world so a given sector's center line points
// straight up -- this is how each player "sees themselves at the top". For
// sector i of N, set angle = rotation_i (= 2*pi*i/N) and the portal edge appears
// up, the nexus down. angle = 0 is the unrotated overview.
struct Camera {
    game::WorldPos center{0.0, 0.0}; // world point shown at viewport center
    double angle = 0.0;              // world rotation applied before projection
    double pixels_per_unit = 8.0;    // zoom
    int viewport_w = 640;
    int viewport_h = 480;

    struct Screen {
        float x = 0.0f;
        float y = 0.0f;
    };

    Screen world_to_screen(game::WorldPos p) const {
        const double dx = p.x - center.x;
        const double dy = p.y - center.y;
        const double c = std::cos(angle);
        const double s = std::sin(angle);
        // rotate by `angle`, then scale, then flip Y for screen space
        const double rx = dx * c - dy * s;
        const double ry = dx * s + dy * c;
        return Screen{
            static_cast<float>(viewport_w * 0.5 + rx * pixels_per_unit),
            static_cast<float>(viewport_h * 0.5 - ry * pixels_per_unit)};
    }

    // Inverse: a viewport pixel back to a world position (used by clicks).
    game::WorldPos screen_to_world(double sx, double sy) const {
        const double rx = (sx - viewport_w * 0.5) / pixels_per_unit;
        const double ry = -(sy - viewport_h * 0.5) / pixels_per_unit;
        const double c = std::cos(-angle);
        const double s = std::sin(-angle);
        const double dx = rx * c - ry * s;
        const double dy = rx * s + ry * c;
        return game::WorldPos{dx + center.x, dy + center.y};
    }

    // Fit a circle of `world_radius` around `c` into the viewport with margin.
    static Camera fit(game::WorldPos c, double world_radius, double angle,
                      int vw, int vh, double margin = 1.12) {
        Camera cam;
        cam.center = c;
        cam.angle = angle;
        cam.viewport_w = vw;
        cam.viewport_h = vh;
        const double span = world_radius * 2.0 * margin;
        const double ppu_w = vw / span;
        const double ppu_h = vh / span;
        cam.pixels_per_unit = ppu_w < ppu_h ? ppu_w : ppu_h;
        return cam;
    }
};

} // namespace mad::rendering
