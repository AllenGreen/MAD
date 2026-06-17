#include "rendering/renderer.hpp"

#include <array>
#include <cmath>
#include <vector>

namespace mad::rendering {

using game::CellCoord;
using game::CellState;
using game::WorldPos;

namespace {

SDL_Vertex vtx(Camera::Screen s, SDL_Color c) {
    SDL_Vertex v;
    v.position = SDL_FPoint{s.x, s.y};
    v.color = c;
    v.tex_coord = SDL_FPoint{0.0f, 0.0f};
    return v;
}

void fill_quad(SDL_Renderer* r, Camera::Screen a, Camera::Screen b,
               Camera::Screen c, Camera::Screen d, SDL_Color col) {
    const SDL_Vertex verts[4] = {vtx(a, col), vtx(b, col), vtx(c, col), vtx(d, col)};
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(r, nullptr, verts, 4, idx, 6);
}

void fill_circle(SDL_Renderer* r, const Camera& cam, WorldPos center,
                 double world_radius, SDL_Color col, int segments = 16) {
    const Camera::Screen c = cam.world_to_screen(center);
    std::vector<SDL_Vertex> verts;
    verts.reserve(segments + 1);
    verts.push_back(vtx(c, col));
    const float rad = static_cast<float>(world_radius * cam.pixels_per_unit);
    for (int i = 0; i < segments; ++i) {
        const double t = 2.0 * M_PI * i / segments;
        Camera::Screen p{c.x + rad * static_cast<float>(std::cos(t)),
                         c.y + rad * static_cast<float>(std::sin(t))};
        verts.push_back(vtx(p, col));
    }
    std::vector<int> idx;
    idx.reserve(segments * 3);
    for (int i = 0; i < segments; ++i) {
        idx.push_back(0);
        idx.push_back(1 + i);
        idx.push_back(1 + (i + 1) % segments);
    }
    SDL_RenderGeometry(r, nullptr, verts.data(), static_cast<int>(verts.size()),
                       idx.data(), static_cast<int>(idx.size()));
}

// World-space thick segment, drawn as a quad of half-width `hw` world units.
void thick_segment(SDL_Renderer* r, const Camera& cam, WorldPos a, WorldPos b,
                   double hw, SDL_Color col) {
    double dx = b.x - a.x, dy = b.y - a.y;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-9) return;
    const double nx = -dy / len * hw;
    const double ny = dx / len * hw;
    fill_quad(r, cam.world_to_screen({a.x + nx, a.y + ny}),
              cam.world_to_screen({b.x + nx, b.y + ny}),
              cam.world_to_screen({b.x - nx, b.y - ny}),
              cam.world_to_screen({a.x - nx, a.y - ny}), col);
}

SDL_Color demon_color(game::MoveType mt) {
    switch (mt) {
        case game::MoveType::Ground:  return SDL_Color{220, 70, 60, 255};
        case game::MoveType::Climber: return SDL_Color{235, 150, 50, 255};
        case game::MoveType::Flyer:   return SDL_Color{120, 200, 250, 255};
        case game::MoveType::Smasher: return SDL_Color{180, 90, 210, 255};
    }
    return SDL_Color{220, 70, 60, 255};
}

} // namespace

void Renderer::draw(SDL_Renderer* r, const Camera& cam, const core::World& world,
                    const Trails* trails) const {
    SDL_SetRenderDrawColor(r, style_.background.r, style_.background.g,
                           style_.background.b, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    const game::GameMap& map = world.map();
    for (int i = 0; i < map.num_sectors(); ++i)
        draw_sector(r, cam, map.sector(i));
    for (int i = 0; i < map.num_sectors(); ++i)
        draw_crystal(r, cam, map.crystal_world(i));
    draw_boundary_walls(r, cam, world);
    if (trails) {
        for (const auto& [id, pts] : *trails) {
            for (std::size_t i = 1; i < pts.size(); ++i) {
                const float a = 30.0f + 110.0f * static_cast<float>(i) / pts.size();
                thick_segment(r, cam, pts[i - 1], pts[i], 0.07,
                              SDL_Color{120, 200, 255, static_cast<uint8_t>(a)});
            }
        }
    }
    draw_nexus(r, cam);
    for (const core::Demon& d : world.demons())
        draw_demon(r, cam, d);
}

void Renderer::draw_boundary_walls(SDL_Renderer* r, const Camera& cam,
                                   const core::World& world) const {
    const game::GameMap& map = world.map();
    for (const core::BoundaryWall& bw : world.boundary_walls()) {
        // The seam between a and b is the radial line at their mid angle; draw a
        // short radial segment at the sealed ring's radius.
        double diff = map.sector(bw.b).rotation() - map.sector(bw.a).rotation();
        while (diff > M_PI) diff -= 2.0 * M_PI;
        while (diff < -M_PI) diff += 2.0 * M_PI;
        const double phi = map.sector(bw.a).rotation() + diff * 0.5;
        const double d = bw.index; // ring = radius
        const WorldPos mid{d * std::sin(phi), d * std::cos(phi)};
        const WorldPos dir{std::sin(phi), std::cos(phi)}; // radial
        const double hw = bw.height >= 2 ? 0.22 : 0.14;
        const double L = 0.6;
        thick_segment(r, cam, {mid.x - dir.x * L, mid.y - dir.y * L},
                      {mid.x + dir.x * L, mid.y + dir.y * L}, hw,
                      style_.border_along);
    }
}

void Renderer::draw_sector(SDL_Renderer* r, const Camera& cam,
                           const game::Sector& sector) const {
    const game::Grid& g = sector.grid();

    // Cells are polar quads: outer-left, outer-right, inner-right, inner-left.
    for (int row = 0; row < g.height(); ++row) {
        for (int col = 0; col < g.width(); ++col) {
            const CellCoord cell{col, row};
            const CellState st = g.cell_state(cell);
            if (st == CellState::Blocked) continue;
            SDL_Color fill = style_.cell_empty;
            if (st == CellState::Tower) fill = style_.cell_tower;
            else if (st == CellState::Boundary) fill = style_.cell_boundary;
            WorldPos cn[4];
            sector.cell_corners(cell, cn);
            fill_quad(r, cam.world_to_screen(cn[0]), cam.world_to_screen(cn[1]),
                      cam.world_to_screen(cn[2]), cam.world_to_screen(cn[3]), fill);
        }
    }

    // Walls follow grid edges: Horizontal = the cell's outer arc, Vertical = its
    // (radial) left edge -- so walls render as smooth arcs and radial lines.
    for (const auto& [edge, data] : g.walls().all()) {
        WorldPos cn[4];
        sector.cell_corners(edge.cell, cn); // 0 outer-L, 1 outer-R, 2 inner-R, 3 inner-L
        const SDL_Color col = data.height >= 2 ? style_.wall_tall : style_.wall_short;
        const double hw = data.height >= 2 ? 0.16 : 0.10;
        switch (edge.type) {
            case game::EdgeType::Horizontal: thick_segment(r, cam, cn[0], cn[1], hw, col); break;
            case game::EdgeType::Vertical:   thick_segment(r, cam, cn[0], cn[3], hw, col); break;
            default:                         thick_segment(r, cam, cn[1], cn[3], hw, col); break;
        }
    }
}

void Renderer::draw_crystal(SDL_Renderer* r, const Camera& cam, WorldPos w) const {
    // Glow + diamond.
    SDL_Color glow = style_.crystal;
    glow.a = 60;
    fill_circle(r, cam, w, 1.6, glow, 20);
    const double s = 0.9;
    fill_quad(r, cam.world_to_screen({w.x, w.y + s}),
              cam.world_to_screen({w.x + s, w.y}),
              cam.world_to_screen({w.x, w.y - s}),
              cam.world_to_screen({w.x - s, w.y}), style_.crystal);
}

void Renderer::draw_nexus(SDL_Renderer* r, const Camera& cam) const {
    SDL_Color glow = style_.nexus;
    glow.a = 70;
    fill_circle(r, cam, {0, 0}, 3.2, glow, 24);
    fill_circle(r, cam, {0, 0}, 1.8, style_.nexus, 24);
    fill_circle(r, cam, {0, 0}, 0.9, SDL_Color{255, 250, 230, 255}, 20);
}

void Renderer::draw_demon(SDL_Renderer* r, const Camera& cam,
                          const core::Demon& d) const {
    const double radius = 0.35 * d.size;
    fill_circle(r, cam, d.pos, radius, demon_color(d.move_type), 14);
    // Thin shard-progress ring tint: brighter core as shards accumulate.
    SDL_Color core{255, 255, 255, 90};
    fill_circle(r, cam, d.pos, radius * 0.4, core, 10);
}

} // namespace mad::rendering
