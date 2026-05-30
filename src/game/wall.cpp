#include "game/wall.hpp"

namespace mad::game {

EdgeCoord WallSet::normalize(EdgeCoord edge) {
    // Canonical form: remap each edge type so the anchor cell
    // is deterministic for the same physical edge.
    //
    // Horizontal edge between (col, row) and (col, row-1):
    //   Always store with the lower row as anchor.
    //   If we have the "upper" cell referencing down, flip it.
    //   Convention: anchor is the cell with smaller row (i.e., row-1 side).
    //   A horizontal edge at (col, row) means "above row, below row-1".
    //   Normalize: always use the smaller row.
    //
    // Vertical edge between (col, row) and (col-1, row):
    //   Always store with the smaller col as anchor.
    //
    // DiagNE between (col, row) and (col+1, row-1):
    //   The anchor is (col, row) with type DiagNE, OR equivalently
    //   (col+1, row-1) with type DiagSW. We only have DiagNE/DiagNW,
    //   so DiagNE at (col, row) is canonical. The reverse would be
    //   DiagNW at (col+1, row). We pick the one with smaller (row, col).
    //
    // DiagNW between (col, row) and (col-1, row-1):
    //   Similarly, DiagNW at (col, row) or DiagNE at (col-1, row-1).
    //   Pick the one with smaller (row, col).

    switch (edge.type) {
        case EdgeType::Horizontal: {
            // Horizontal at (c, r) = edge between (c, r) and (c, r-1)
            // Canonical: use the cell with the smaller row
            // The "other side" would be Horizontal at (c, r-1+1) = (c, r), so
            // there's no ambiguity as long as we define the convention:
            // Horizontal at (c, r) always means the edge on the TOP side of (c, r).
            // No flip needed — each horizontal edge has a unique (col, row) anchor.
            return edge;
        }
        case EdgeType::Vertical: {
            // Vertical at (c, r) = edge between (c, r) and (c-1, r)
            // This means the edge on the LEFT side of (c, r).
            // No flip needed — each vertical edge has a unique (col, row) anchor.
            return edge;
        }
        case EdgeType::DiagNE: {
            // DiagNE at (c, r) connects (c, r) and (c+1, r-1)
            // The same edge is DiagNW at (c+1, r-1) connecting to (c, r)
            // Wait — DiagNW at (c+1, r-1) connects (c+1, r-1) and (c+1-1, r-1-1) = (c, r-2).
            // That's not the same edge!
            //
            // Let me reconsider. DiagNE at (c, r) = edge between (c,r) and (c+1, r-1).
            // From (c+1, r-1)'s perspective, looking toward (c, r) = that's southwest.
            // We don't have DiagSW. Instead, we represent it as:
            // DiagNW at (c+1, r) would connect (c+1, r) and (c, r-1) — also not the same.
            //
            // Actually the simplest normalization: for DiagNE at (c,r) connecting
            // (c,r)↔(c+1,r-1), pick the endpoint with smaller row, then smaller col.
            // (c+1, r-1) has smaller row, so anchor = (c+1, r-1), but we need to
            // express this as a valid EdgeType from that anchor.
            // From (c+1, r-1), going to (c, r) = going (-1, +1) = that's DiagSW.
            // Since we don't have DiagSW, we just always use the DiagNE form
            // with the lower-left cell as anchor. That IS (c, r) when r > r-1.
            // So DiagNE is already canonical when anchored at the cell with larger row.
            return edge;
        }
        case EdgeType::DiagNW: {
            // DiagNW at (c, r) connects (c, r) and (c-1, r-1).
            // Similar reasoning — already canonical with the larger-row cell as anchor.
            return edge;
        }
    }
    return edge;
}

void WallSet::add(EdgeCoord edge, WallData data) {
    walls_[normalize(edge)] = data;
}

void WallSet::remove(EdgeCoord edge) {
    walls_.erase(normalize(edge));
}

bool WallSet::has(EdgeCoord edge) const {
    return walls_.contains(normalize(edge));
}

const WallData* WallSet::get(EdgeCoord edge) const {
    auto it = walls_.find(normalize(edge));
    return it != walls_.end() ? &it->second : nullptr;
}

bool WallSet::blocks_ground(EdgeCoord edge) const {
    return has(edge); // any wall blocks ground
}

bool WallSet::blocks_climber(EdgeCoord edge) const {
    auto* w = get(edge);
    return w && w->height >= 2; // only tall walls block climbers
}

} // namespace mad::game
