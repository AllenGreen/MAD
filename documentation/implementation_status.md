# MAD — Implementation Status & Handoff

This document is the authoritative snapshot of what is built right now, what is only designed, and where to start next.

**Last updated**: 2026-06-13 (after adding Summoning Crystal positions)

For *what the game is*, read `game_design.md`. For *system shape*, read `architecture.md`. This file is the bridge between design and code.

---

## 1. Build & Run

Canonical entry points — never invoke cmake/ctest/ninja directly:

| Script | Purpose |
|---|---|
| `./build.sh [Debug\|Release\|clean]` | Configure + build. Debug is default and enables `-fsanitize=address,undefined`. Auto-prefers Ninja. |
| `./test.sh [regex]` | Build + run `ctest`. Optional regex passes through as `-R`. |
| `./run.sh [args...]` | Build + launch the `mad` binary. Args forwarded. |

Build artifacts live in `build/`. `.gitignore` excludes them.

**Toolchain assumptions** (already verified to work on user's box):
- C++23 (uses `std::format`, `std::span`, `std::format_string`)
- CMake ≥ 3.22, Ninja preferred
- SDL2 dev headers (`libsdl2-dev` on Ubuntu)
- Compiler warnings are `-Wall -Wextra -Wpedantic -Werror=return-type`

## 2. Repository Layout

```
MAD/
├── CMakeLists.txt
├── build.sh, test.sh, run.sh
├── documentation/
│   ├── game_design.md            # Lore, mechanics, what the game IS
│   ├── architecture.md           # Modules, threads, tech stack
│   └── implementation_status.md  # THIS FILE
├── include/
│   ├── core/    (engine, log, test framework)
│   ├── game/    (grid_types, grid, wall, sector, game_map, pathfinding, flow_field)
│   ├── network/ (socket)
│   └── rendering/ (window)
├── src/         (mirrors include/)
├── tests/       (test_grid, test_wall, test_sector, test_pathfinding, test_main)
└── build/       (gitignored)
```

There are no asset, shader, or third-party-source directories yet.

## 3. What's Implemented vs. Designed

### Fully implemented and tested

#### 3.1 Core (`mad::core`)
- `Engine` — fixed-timestep loop scaffolding. **Run loop is currently a stub**; it initializes SDL, opens a window, processes events, but does no game updates yet (`update(dt)` and `render()` are empty).
- `log` — tagged levels (Debug/Info/Warn/Error) backed by `std::format` and `fprintf(stderr)`.
- `test.hpp` — minimal registry-based framework: `TEST(name) { ... }`, `ASSERT_TRUE`, `ASSERT_EQ`. Each test returns `bool` and runs via `mad::test::run_all()` (called from `tests/test_main.cpp`).

#### 3.2 Game grid layer (`mad::game`)
- `grid_types.hpp` — primitive types:
  - `CellCoord { int col, row }` with `==`/`<=>` and `CellCoordHash`. **Note: for multi-size units, CellCoord is the top-left of the footprint.**
  - `WorldPos { double x, y }` — world space, nexus at origin, +Y up, +X right.
  - `enum CellState : uint8_t { Empty, Tower, Blocked, Boundary }`
  - `enum EdgeType { Horizontal, Vertical, DiagNE, DiagNW }` — wall edges sit *between* cells.
  - `EdgeCoord { CellCoord cell, EdgeType type }` with hash.
  - `enum MoveType { Ground, Climber, Flyer, Smasher }`
- `WallSet` — sparse `unordered_map<EdgeCoord, WallData>` of walls.
  - `WallData { uint8_t height, uint8_t hp }`. `height=1` is short (climbable), `2+` is tall.
  - `normalize(EdgeCoord)` — same physical edge always maps to the same key (e.g. `(c,r,Horizontal)` and `(c, r-1, Horizontal-from-below)` collapse).
  - `blocks_ground(edge)` returns true for any wall. `blocks_climber(edge)` returns true only for tall walls.
- `Grid` — owns a `vector<CellState>` (row-major) + `unique_ptr<WallSet>`.
  - `in_bounds`, `cell_state/set_cell_state`, `is_walkable` (`Empty` or `Boundary` only).
  - `is_footprint_walkable(origin, size)` — size×size footprint check.
  - `can_move(from, to)` and size-aware `can_move(from, to, size)`:
    - 8-directional adjacency check
    - Walkability of both endpoints (or footprints)
    - Corner-cutting prevention: cardinal walls + footprint blockage stop diagonals
    - Diagonal *tower* corners block "squeezing through" two diagonally placed towers
  - `walls_block_move(from, to)` (private) handles the edge-set logic, including:
    - Cardinal moves check the single leading edge (or S edges for size S)
    - Diagonal moves check the matching `DiagNE`/`DiagNW` edge AND both adjacent cardinal edges (if both are walled, the L-shape blocks the diagonal)
  - `walkable_neighbors(cell, out[8])` and size-aware overload — writes valid moves into caller's buffer.

#### 3.3 Geometry (`mad::game`)
- `Sector` — one player's wedge. Owns its own `Grid`.
  - Constructor takes `(player_id, grid_w, grid_h, rotation_rad, half_angle, map_radius, cell_size)`.
  - **Grid orientation**: row 0 = portal edge (outermost), last row = nexus side (innermost). Column 0 = left, last column = right. The grid is **centered horizontally** on the sector's center line; the top-center of the grid aligns with `(0, map_radius)` in unrotated space.
  - **Rotation convention**: angle from `+Y` axis, clockwise. Sector *i* of *N* has `rotation = 2π·i/N`. Player 0 always sits at "the top of the world" (rotation 0).
  - `cell_to_world(cell)` / `world_to_cell(pos)` — transform with cached `cos`/`sin`.
  - `contains_world(pos)` — angle-based test with `±half_angle` window.
  - `portal_cells()` — walkable cells in row 0.
  - `nexus_cells()` — walkable cells in last row.
  - **`crystal_cell()` — returns `(width/2, height/2)`** (just added). Mid-wedge by construction.
  - **`crystal_world()` — world position of the crystal cell**.
  - `mask_out_of_wedge_cells()` — runs at construction, marks any cell whose center lies outside the wedge as `CellState::Blocked`.
- `GameMap` — owns `vector<Sector>`. Constructed from a `MapConfig { num_players, grid_width, grid_height, map_radius, cell_size }`.
  - `sector(id)`, `num_sectors()`, `config()`.
  - `sector_at(WorldPos)` — returns the player id whose wedge contains the point, or -1.
  - `transform_cell(cell, from, to)` — via world space.
  - `boundary_cells(a, b)` — walks the shared boundary line outward from the nexus and returns paired `(cell_a, cell_b)` in each grid's local coords.
  - `portal_cells(id)`, `nexus_cells(id)`.
  - **`crystal_cell(id)`, `crystal_world(id)`** (just added).

#### 3.4 Pathfinding (`mad::game::Pathfinder`)
- A* within a single sector. Octile-distance heuristic (admissible for 8-direction grids with √2 diagonal cost).
- `PathRequest { start, goal, sector_id, MoveType, unit_size=1, max_steps=10000 }`.
- `PathResult { found, cells, waypoints, cost }` — `cells` is the raw grid path; `waypoints` is the smoothed funnel/string-pull path in world space.
- Move-type semantics:
  - **Ground**: respects walls via `can_move` size-aware.
  - **Climber**: same, but treats short walls (`height==1`) as passable (`WallSet::blocks_climber` returns false there).
  - **Flyer**: ignores walls and `Tower`/`Boundary` — only checks `is_footprint_walkable` for the footprint, and inside-grid-bounds.
  - **Smasher**: like Ground but adds `SMASHER_WALL_PENALTY = 5.0` to walled edges instead of refusing — used for tower-targeting pathing once enrage is hooked up.
- **Cross-sector pathing**: `find_path_cross_sector` chains A* through `boundary_cells()` pairs — picks the cheapest boundary then runs A* on each side. Not yet used by gameplay code; tested directly.
- **String-pulling**: Bresenham-LoS-based funnel smoothing converts the cell path into world-space waypoints. Size-aware: each LoS step validates the footprint, not just one cell.

#### 3.5 Flow fields (`mad::game::FlowField`)
- Dijkstra flood from goals out. Goals can be a single `CellCoord` or `span<const CellCoord>`.
- Per-`(MoveType, unit_size)` field. Stores `cost_field_` (doubles, `INF` = `UNREACHABLE`) and `direction_field_` (best neighbor per cell).
- `best_neighbor(cell)` for steering; `cost_at(cell)` for goal-distance queries.
- 8-directional with √2 diagonal cost. Movement passability uses the same Grid checks as A* (size-aware).
- `valid_` flag with `invalidate()` for future regeneration hooks. **No manager yet — nobody owns or refreshes these fields**.

#### 3.6 Tests
- `test_main.cpp` invokes `mad::test::run_all()`.
- **68 tests passing, 0 failing** as of this writing.
- Coverage: grid movement / footprints / corner-cutting; wall edge normalization & blocking; sector creation / rotation / world transforms / wedge masking / boundary cells / 5-player parametrics; A* one-sector and cross-sector / string-pull / size; flow field generation and size-difference; **crystal mid-wedge cell, walkability across N=3..6, distinct world positions per sector, distance between portal and nexus**.

### Stubbed (compiles, doesn't do anything yet)
- `core::Engine::run` — main loop turns over but the update/render bodies are empty.
- `rendering::Window` — wraps SDL2 window + renderer; nothing draws.
- `network::UDPSocket` — raw blocking-mode UDP send/recv; **no reliability layer, no game-state serialization, no peer discovery**.

### Designed but not implemented at all
*Everything below has design notes in `game_design.md` and architecture lines in `architecture.md`, but zero code.*

- Crystal as a **structure** — currently only a position. No `CellState::Crystal`, no footprint reservation, no shard count, no HP.
- **Flow field manager** — owning N+1 fields per `(unit_size, MoveType)` combo (one per crystal + one for nexus), regenerating on wall/tower placement, exposing "cost from cell X to crystal Y" for boundary transition costs.
- **Cross-sector flow-field handoff** with cost-to-goal at boundaries (the "Option B" design choice — per-sector fields with cost passed across boundary cells).
- **Demon unit** — `shards_collected`, `wave_direction (CW/CCW)`, current goal, position interpolation, size, MoveType, HP.
- **Wave manager** — spawn schedule, alternation of CW/CCW per wave, portal-edge selection (which portals spawn this wave).
- **Demon enrage** — none of the 4 candidate strategies have been implemented (see `game_design.md` §Demon Enrage). Also no path-blockage detection, no obstruction identification, no anti-ping-pong (travel-budget or backtrack-threshold).
- **Tower system** — placement, upgrade-in-place, merge of 3×1x1 → larger.
- **Magic schools, hybrids, specialization progression** — pure design.
- **Projectile system, damage model**.
- **Multiplayer (lobby, sync, peer host)**.
- **Renderer** — no grid drawing, no per-sector rotation, no sprite system, no UI.
- **Demon tech progression** (era-based wave difficulty).
- **Threading model from architecture.md** — currently single-threaded.

## 4. Coordinate Systems & Conventions

These are easy to get wrong; double-check before changing geometry code.

### World space
- 2D, doubles, units = "world units". `cell_size=1.0` in current `MapConfig`, so 1 world unit ≈ 1 cell.
- **Origin = the Nexus** (map center).
- `+Y` axis points "up" (toward player 0's portal). `+X` axis points right.
- Angles are measured *from `+Y`, clockwise*. This matches how sectors are rotated and how `Sector::contains_world` uses `atan2(pos.x, pos.y)` rather than the more typical `atan2(y, x)`.

### Sector local space
- A sector has a `grid_width × grid_height` integer grid.
- Local `(lx, ly)`: lx grows right, ly grows *inward* (toward nexus). So `ly = 0` is the portal edge; `ly = grid_height * cell_size` is at the nexus side.
- `cell_to_world(cell)` returns the center of the cell (`+0.5` in both axes).
- Cells whose center falls outside the wedge polygon are marked `CellState::Blocked` at construction.

### Sector layout in the polygon
- Polygon is N-sided. Sector `i` has rotation `2π·i/N` (CW from `+Y`). `half_angle = π/N` (each sector covers `2π/N` total).
- The grid is **centered on the sector's center line**, with row 0 sitting at distance `map_radius` from origin. So the player's portal edge runs along an arc of length `~2·map_radius·sin(π/N)`. For wide grids and small N this means much of the grid hangs outside the wedge and gets masked — see `sector_wedge_masking` test for the sizing math.

### Multi-size footprint
- `CellCoord` for a sized unit refers to the **top-left** of its footprint. A size=2 unit at `(c,r)` occupies `{(c,r), (c+1,r), (c,r+1), (c+1,r+1)}`.
- Movement is still single-cell 8-directional even for large units.
- Walls block multi-size cardinal moves if *any* of the S edges along the leading face is walled; diagonal moves additionally check all S leading edges on *both* axes plus all S diagonal edges, plus corner-cutting (all-cardinal-on-both-axes → blocked).

### Wall edges (between cells)
- An `EdgeCoord` is `(CellCoord cell, EdgeType type)` and represents an edge attached to that cell.
- `Horizontal` = edge between `(c,r)` and `(c, r-1)` — i.e. the *top* edge of the named cell.
- `Vertical` = edge between `(c,r)` and `(c-1, r)` — i.e. the *left* edge of the named cell.
- `DiagNE` and `DiagNW` = the two diagonal edge orientations.
- `WallSet::normalize` canonicalizes any equivalent reference (e.g. "bottom of cell A" == "top of cell B") to a single key. Always normalize before lookup/storage.

## 5. Known Pitfalls Hit During Development

These are written down so the next agent doesn't waste cycles re-discovering them:

- **Wedge masking can eat your grid.** For high N (e.g. 6 players → 30° half-angle), narrow wedges mean cells far from the center line get masked Blocked. Tests that need cells near the edge of the grid must use a wider grid OR a smaller N. See `sector_wedge_masking` and `sector_boundary_cells_adjacent` for the sizing pattern.
- **Boundary line can fall outside narrow grids.** Boundary at distance *d* along an angle has local |x| ≈ `d·sin(half_angle)`. The grid half-width (`width·cell_size/2`) must exceed the maximum boundary |x| to find pairs. With 4 players, boundary angle is 45°, so you need `width > √2·map_radius` for full coverage.
- **Diagonal moves require diagonal edges to block fully.** Horizontal/vertical walls alone don't block diagonals; you need a `DiagNE` or `DiagNW`, *or* the L-shaped pair that triggers corner-cutting. `pathfinding_wall_edge_blocks` had to add the diagonal walls explicitly.
- **Wall edges normalize.** Don't compare raw EdgeCoords; go through `WallSet::has` (which normalizes internally) or call `WallSet::normalize` yourself.
- **`Sector::cell_to_world` rotates around origin (the nexus), not the grid center.** Don't confuse "sector center line" with "rotation pivot."

## 6. Open Design Decisions (in priority order)

These were captured in `game_design.md` as prospects to track, not as decided. The next agent should expect to discuss before implementing:

1. **Crystal as a cell occupant.** Currently the crystal is a position only. Options:
   - Add `CellState::Crystal` — explicit, cleanly distinguishable from towers.
   - Reuse `CellState::Tower` with side metadata — fewer states, but conflates two concepts.
   - Crystal footprint size: 1×1, 2×2, 3×3? Affects placement budget and wall geometry around it.
2. **Flow field ownership and refresh.** Who calls `FlowField::generate`? When does it run (on placement? batched per tick?)? How are the N+1 fields per (size, move_type) keyed and stored?
3. **Demon enrage strategy.** Four candidates designed (smash most-recent obstruction, path-toward-boundary-then-smash, smash nearest, hybrid). Picking one requires choosing an obstruction-identification mechanism (path invalidation events → who-placed-what tracking).
4. **Anti-ping-pong measure.** Max-travel-budget vs. backtrack-distance threshold. Both need per-demon state.
5. **Wave direction (CW/CCW) toggling.** Trivial state, but needs wave-manager scaffolding first.
6. **Multiplayer architecture.** Peer-hosted vs. authoritative server. Affects pathing determinism requirements.

## 7. Recommended Next Steps

Reasonable orderings, given dependencies:

**Path A — Build out gameplay loop**
1. Add `CellState::Crystal` (or chosen alternative). Mark crystal cells at sector construction.
2. Build a `FlowFieldManager` that owns N+1 fields per `(MoveType, unit_size)` for one map, with `regenerate_all()` and a per-goal `field_for(goal_idx, mt, size)`.
3. Implement a minimal `Demon` struct + `WaveManager` that spawns demons at a portal and steps them via flow-field steering.
4. **Then** start on enrage / anti-ping-pong (needs demons to ping-pong before it's testable).

**Path B — Get something on screen first**
1. Flesh out `Engine::update`/`render` to call into a `Renderer` that draws each `Sector`'s grid with its rotation.
2. Render crystals as a visible marker.
3. Add a debug overlay for flow-field cost / direction.
4. *Then* gameplay loop.

**Path C — Lock down core infrastructure**
1. Build the threading model from `architecture.md` (game logic on one thread, pathfinding on another).
2. Build the UDP reliability layer (sequence numbers, ACKs, simple replay window).
3. Build game-state serialization.

The user has not picked a path. Confirm before committing to one.

## 8. House Rules

- **Always go through `build.sh`/`test.sh`/`run.sh`**. Do not invoke cmake or ctest directly. Update the helpers if you need new functionality.
- **Tests must pass before declaring done.** Run `./test.sh`. There are no other CI gates yet.
- **C++23 only.** No backwards-compat shims, no `#ifdef` ladders for older standards.
- **Engine namespaces**: `mad::core`, `mad::game`, `mad::network`, `mad::rendering`, `mad::test`.
- **No new top-level docs unless asked.** Edit `game_design.md`/`architecture.md`/this file in place.
- **No emojis in code, commits, or docs unless explicitly requested.**

## 9. Memory & Long-term Context

Auto-memory lives at `/home/allen/.claude/projects/-home-allen-Projects-MAD/memory/`. The two existing entries:
- `project_mad_overview.md` — game concept, grid decision, crystal/shard mechanic, enrage prospects, multi-size units, tech stack.
- `feedback_use_helper_scripts.md` — always use build/test/run.sh.

If you learn new project-shaping facts or get feedback on approach, update memory.

## 10. Git State

- Branch: `master` (the repo's only branch). Remote `origin` = `git@github.com:AllenGreen/MAD.git`.
- Single root commit `0dd97d2` ("Initial commit: MAD tower defense engine scaffolding").
- Pending work since that commit: crystal positions on `Sector`/`GameMap` and the 4 new tests — not yet committed.
