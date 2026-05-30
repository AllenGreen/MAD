#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

namespace mad::game {

struct CellCoord {
    int col = 0;
    int row = 0;

    bool operator==(const CellCoord&) const = default;
    auto operator<=>(const CellCoord&) const = default;
};

struct WorldPos {
    double x = 0.0;
    double y = 0.0;
};

enum class CellState : uint8_t {
    Empty    = 0,
    Tower    = 1,
    Blocked  = 2,
    Boundary = 3
};

enum class EdgeType : uint8_t {
    Horizontal, // between (col,row) and (col, row-1)
    Vertical,   // between (col,row) and (col-1, row)
    DiagNE,     // between (col,row) and (col+1, row-1)
    DiagNW      // between (col,row) and (col-1, row-1)
};

struct EdgeCoord {
    CellCoord cell;
    EdgeType type = EdgeType::Horizontal;

    bool operator==(const EdgeCoord&) const = default;
};

struct CellCoordHash {
    std::size_t operator()(const CellCoord& c) const noexcept {
        auto h1 = std::hash<int>{}(c.col);
        auto h2 = std::hash<int>{}(c.row);
        return h1 ^ (h2 << 16) ^ (h2 >> 16);
    }
};

enum class MoveType : uint8_t {
    Ground  = 0,
    Climber = 1,
    Flyer   = 2,
    Smasher = 3
};

struct EdgeCoordHash {
    std::size_t operator()(const EdgeCoord& e) const noexcept {
        auto h1 = CellCoordHash{}(e.cell);
        auto h2 = std::hash<uint8_t>{}(static_cast<uint8_t>(e.type));
        return h1 ^ (h2 * 2654435761u);
    }
};

} // namespace mad::game
