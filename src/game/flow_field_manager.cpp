#include "game/flow_field_manager.hpp"
#include "core/log.hpp"

#include <cmath>

namespace mad::game {

const FlowField& FlowFieldManager::build(uint64_t k, int sector, Goal goal,
                                         int neighbor, MoveType mt, int size) {
    const Grid& grid = map_.sector(sector).grid();
    auto [it, inserted] = fields_.try_emplace(k, grid, grid.width(), grid.height());
    FlowField& field = it->second;
    if (!inserted && field.is_valid())
        return field;

    switch (goal) {
        case Goal::Crystal: {
            field.generate(map_.crystal_cell(sector), mt, size);
            break;
        }
        case Goal::Nexus: {
            // The grid's literal last row is masked away where the wedge pinches
            // to a point at the origin, so use the walkable cells physically
            // nearest the Nexus (world origin) as the goal instead.
            const Grid& g = map_.sector(sector).grid();
            const Sector& s = map_.sector(sector);
            std::vector<CellCoord> goals;
            CellCoord nearest{};
            double best = 1e30;
            for (int row = 0; row < g.height(); ++row)
                for (int col = 0; col < g.width(); ++col) {
                    CellCoord c{col, row};
                    if (!g.is_walkable(c)) continue;
                    const WorldPos w = s.cell_to_world(c);
                    const double d2 = w.x * w.x + w.y * w.y;
                    if (d2 < best) { best = d2; nearest = c; }
                    if (d2 <= 6.25) goals.push_back(c); // within 2.5 world units
                }
            if (goals.empty()) goals.push_back(nearest);
            core::log::debug("FlowFieldMgr", "nexus field s{}: {} goal cells (nearest d={:.2f})",
                             sector, goals.size(), std::sqrt(best));
            field.generate(goals, mt, size);
            break;
        }
        case Goal::Boundary: {
            const auto pairs = map_.boundary_cells(sector, neighbor);
            std::vector<CellCoord> goals;
            goals.reserve(pairs.size());
            for (std::size_t i = 0; i < pairs.size(); ++i) {
                if (boundary_blocked(sector, neighbor, static_cast<int>(i)))
                    continue; // an "along" wall seals this crossing
                goals.push_back(pairs[i].cell_a);
            }
            core::log::debug("FlowFieldMgr", "boundary field s{}->s{}: {}/{} open goal cells",
                             sector, neighbor, goals.size(), pairs.size());
            field.generate(goals, mt, size);
            break;
        }
    }
    return field;
}

void FlowFieldManager::set_boundary_blocked(int a, int b, int index, bool blocked) {
    auto& flags = blocked_crossings_[pair_key(a, b)];
    if (static_cast<int>(flags.size()) <= index)
        flags.resize(index + 1, 0);
    flags[index] = blocked ? 1 : 0;
}

bool FlowFieldManager::boundary_blocked(int a, int b, int index) const {
    auto it = blocked_crossings_.find(pair_key(a, b));
    if (it == blocked_crossings_.end()) return false;
    return index >= 0 && index < static_cast<int>(it->second.size()) && it->second[index];
}

const FlowField& FlowFieldManager::crystal_field(int sector, MoveType mt, int size) {
    return build(key(sector, Goal::Crystal, 0, mt, size), sector, Goal::Crystal, 0, mt, size);
}

const FlowField& FlowFieldManager::nexus_field(int sector, MoveType mt, int size) {
    return build(key(sector, Goal::Nexus, 0, mt, size), sector, Goal::Nexus, 0, mt, size);
}

const FlowField& FlowFieldManager::boundary_field(int sector, int neighbor,
                                                  MoveType mt, int size) {
    return build(key(sector, Goal::Boundary, neighbor, mt, size), sector,
                 Goal::Boundary, neighbor, mt, size);
}

// Goal cells nearest the world origin in `sector` (the grid's literal last row is
// masked away where the wedge pinches to a point).
static std::vector<CellCoord> nexus_goal_cells(const GameMap& map, int sector) {
    const Grid& g = map.sector(sector).grid();
    const Sector& s = map.sector(sector);
    std::vector<CellCoord> goals;
    CellCoord nearest{};
    double best = 1e30;
    for (int row = 0; row < g.height(); ++row)
        for (int col = 0; col < g.width(); ++col) {
            CellCoord c{col, row};
            if (!g.is_walkable(c)) continue;
            const WorldPos w = s.cell_to_world(c);
            const double d2 = w.x * w.x + w.y * w.y;
            if (d2 < best) { best = d2; nearest = c; }
            if (d2 <= 6.25) goals.push_back(c);
        }
    if (goals.empty()) goals.push_back(nearest);
    return goals;
}

const GlobalFlowField& FlowFieldManager::global_crystal_field(int target, MoveType mt,
                                                              int size) {
    const uint64_t k = (0ull) | (static_cast<uint64_t>(target) << 4)
                     | (static_cast<uint64_t>(static_cast<uint8_t>(mt)) << 12)
                     | (static_cast<uint64_t>(size) << 20);
    auto it = global_fields_.find(k);
    if (it != global_fields_.end() && it->second->is_valid()) return *it->second;
    auto field = std::make_unique<GlobalFlowField>(map_);
    std::vector<std::pair<int, CellCoord>> goals{{target, map_.crystal_cell(target)}};
    field->generate(goals, mt, size,
                    [this](int a, int b, int i) { return boundary_blocked(a, b, i); });
    auto& ref = *field;
    global_fields_[k] = std::move(field);
    return ref;
}

const GlobalFlowField& FlowFieldManager::global_nexus_field(MoveType mt, int size) {
    const uint64_t k = (1ull) | (static_cast<uint64_t>(static_cast<uint8_t>(mt)) << 12)
                     | (static_cast<uint64_t>(size) << 20);
    auto it = global_fields_.find(k);
    if (it != global_fields_.end() && it->second->is_valid()) return *it->second;
    auto field = std::make_unique<GlobalFlowField>(map_);
    std::vector<std::pair<int, CellCoord>> goals;
    for (int s = 0; s < map_.num_sectors(); ++s)
        for (const CellCoord c : nexus_goal_cells(map_, s))
            goals.push_back({s, c});
    field->generate(goals, mt, size,
                    [this](int a, int b, int i) { return boundary_blocked(a, b, i); });
    auto& ref = *field;
    global_fields_[k] = std::move(field);
    return ref;
}

} // namespace mad::game
