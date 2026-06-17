#pragma once

#include "core/world.hpp"
#include "rendering/camera.hpp"

#include <SDL2/SDL.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace mad::rendering {

// Optional per-demon position history, drawn as fading trails (path debugging).
using Trails = std::unordered_map<uint32_t, std::vector<game::WorldPos>>;

// Draws a World from a Camera onto any SDL_Renderer (window or offscreen target).
// Stateless aside from style constants -- the same Renderer serves every camera.
class Renderer {
public:
    struct Style {
        SDL_Color background{14, 10, 22, 255};
        SDL_Color cell_empty{38, 34, 54, 255};
        SDL_Color cell_tower{120, 110, 90, 255};
        SDL_Color cell_boundary{60, 50, 80, 255};
        SDL_Color grid_line{24, 20, 34, 255};
        SDL_Color wall_tall{210, 180, 120, 255};
        SDL_Color wall_short{120, 100, 70, 255};
        SDL_Color crystal{90, 220, 230, 255};
        SDL_Color nexus{250, 210, 110, 255};
        SDL_Color border_along{240, 90, 90, 255};      // seals a crossing
        SDL_Color border_perp{90, 230, 150, 255};      // seals the seam lane
        bool draw_grid_lines = true;
    };

    void draw(SDL_Renderer* r, const Camera& cam, const core::World& world,
              const Trails* trails = nullptr) const;

private:
    Style style_;

    void draw_sector(SDL_Renderer* r, const Camera& cam,
                     const game::Sector& sector) const;
    void draw_crystal(SDL_Renderer* r, const Camera& cam, game::WorldPos w) const;
    void draw_nexus(SDL_Renderer* r, const Camera& cam) const;
    void draw_demon(SDL_Renderer* r, const Camera& cam, const core::Demon& d) const;
    void draw_boundary_walls(SDL_Renderer* r, const Camera& cam,
                             const core::World& world) const;
};

} // namespace mad::rendering
