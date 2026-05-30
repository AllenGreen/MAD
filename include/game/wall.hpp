#pragma once

#include "game/grid_types.hpp"
#include <unordered_map>

namespace mad::game {

struct WallData {
    uint8_t height = 1;  // 1 = short (climbable), 2+ = tall
    uint8_t hp = 255;
};

class WallSet {
public:
    void add(EdgeCoord edge, WallData data = {});
    void remove(EdgeCoord edge);
    bool has(EdgeCoord edge) const;
    const WallData* get(EdgeCoord edge) const;

    bool blocks_ground(EdgeCoord edge) const;
    bool blocks_climber(EdgeCoord edge) const;

    const auto& all() const { return walls_; }
    void clear() { walls_.clear(); }
    std::size_t size() const { return walls_.size(); }

    // Normalize an edge coord so the same physical edge always maps to the same key.
    static EdgeCoord normalize(EdgeCoord edge);

private:
    std::unordered_map<EdgeCoord, WallData, EdgeCoordHash> walls_;
};

} // namespace mad::game
