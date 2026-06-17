# MAD — Implementation Status & Handoff

This document is the authoritative snapshot of what is built right now, what is only designed, and where to start next.

**Last updated**: 2026-06-14 (after building the AI development harness + first gameplay loop)

For *what the game is*, read `game_design.md`. For *system shape*, read `architecture.md`. For *how to build/run/record the game as an AI agent*, read `ai_harness.md`. This file is the bridge between design and code.

---

## 1. Build & Run

Canonical entry points — never invoke cmake/ctest/ninja directly:

| Script | Purpose |
|---|---|
| `./build.sh [Debug\|Release\|clean]` | Configure + build. Debug is default and enables `-fsanitize=address,undefined`. Auto-prefers Ninja. |
| `./test.sh [regex]` | Build + run `ctest`. Optional regex passes through as `-R`. |
| `./run.sh [args...]` | Build + launch the `mad` binary. Args forwarded. |
| `./record.sh <scenario.mad> [flags]` | **AI harness**: build + headless record + encode video + publish to `/Content`. See `ai_harness.md`. |

The `mad` binary also takes `--record <scenario.mad> --out <dir>` to run headlessly and dump frames (no display, deterministic). `tools/capture.py` turns frames into video and publishes them.

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
├── build.sh, test.sh, run.sh, record.sh
├── documentation/
│   ├── game_design.md            # Lore, mechanics, what the game IS
│   ├── architecture.md           # Modules, threads, tech stack
│   ├── ai_harness.md             # How to build/run/record as an AI agent
│   └── implementation_status.md  # THIS FILE
├── include/
│   ├── core/    (engine, log, test, world, scenario, recorder, serialize)
│   ├── game/    (grid_types, grid, wall, sector, game_map, pathfinding,
│   │             flow_field, flow_field_manager)
│   ├── network/ (socket)
│   └── rendering/ (window, camera, renderer)
├── src/         (mirrors include/)
├── scenarios/   (.mad scenario files — e.g. demo_3p.mad)
├── tools/       (capture.py — frames -> video -> /Content)
├── tests/       (grid, wall, sector, pathfinding, camera, world, main)
└── build/       (gitignored)
```

There are no asset, shader, or third-party-source directories yet. Captured frames
go to `/Data/mad_capture/` (scratch); published reports go to `/Content/`.

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

#### 3.6 Gameplay loop + AI harness (NEW, 2026-06-14)
This is the first end-to-end playable+observable slice. See `ai_harness.md`.
- `core::World` — deterministic, render-free simulation: owns the `GameMap`, a
  `FlowFieldManager`, demons, and wave spawns. Fixed timestep. Inputs (`spawn_wave`,
  `place_wall`, `place_tower`) mimic human actions.
- `game::GlobalFlowField` (2026-06-15) — **whole-map** Dijkstra flood that crosses seams
  (the design's "Option B"). Demons follow ONE field to their goal across all sectors, so
  routing is globally optimal (winds around the ring / reroutes around sealed seams). The
  steering direction is recorded during relaxation, so it's always a legal move (no
  wall-ignoring direction pass). Seam crossings are sampled densely by RADIUS and nudged
  into each sector, so units cross at (nearly) any radius — this is what makes **smooth,
  teleport-free transitions** and tangential travel around a ring possible. `FlowFieldManager`
  caches one global field per goal (each crystal, the Nexus) per `(MoveType, size)`.
- `game::FlowFieldManager` — also still builds the older per-sector fields; the whole-map
  fields above are what demon steering uses. **Nexus goal = walkable cells nearest the world
  origin** (the literal last grid row is masked away where the wedge pinches).
- **Smooth seam crossing** — a demon steers continuously toward the next cell even when it's
  across a seam, and commits `d.sector` on the crossing step (no teleport, no boundary
  oscillation). `World::step_demon`.
- `Demon` — walks the full shard route: spawn portal → its own crystal → each other
  crystal in rotational order (CW/CCW) → Nexus. Cross-sector movement uses a
  boundary field to reach the seam, then a **handoff** (`World::try_cross`) teleports
  it to the paired cell in the neighbor grid. Flyers ignore walls, climbers ignore
  short walls (via the existing move-type flow-field semantics).
- `rendering::Camera` — world↔screen with per-sector rotation ("every player at the
  top"). `rendering::Renderer` — draws sectors (rotated cell quads), walls, crystals,
  Nexus, and demons to any `SDL_Renderer` (window or offscreen) via `SDL_RenderGeometry`.
- `core::Scenario` + parser — dependency-free `.mad` text format (map, cameras, timed
  spawn/wall/tower/maze events) so an agent authors runs declaratively.
- `World::generate_maze` — recursive-backtracker "perfect" maze over a sector's walkable
  cells (tall walls on uncarved edges). Spanning-tree connectivity keeps crystals/nexus/
  boundaries reachable; deterministic per seed. Exposed as the `maze` scenario command
  (`scenarios/maze_3p.mad`). A strong pathing stress test — see the published capture.
- **Border walls** — two build options for shaping paths at the seam where two players'
  coordinate systems meet (`scenarios/border_walls.mad`, published demo with trails):
  - **ALONG** (`World::add_boundary_wall`, `core::BoundaryWall`) — a wall along the seam that
    seals *crossing* at a ring. `FlowFieldManager` drops sealed crossings from boundary-field
    goals, so demons reroute to an open one. `border` scenario command.
  - **PERP rung** (`World::add_rung`) — a tangential wall across a wedge at a grid row, with a
    gap at one border. Alternating gap sides build a **serpentine**: demons zig-zag border to
    border, dropping a ring only at a gap. Rungs are plain grid walls, so normal flow-field
    pathing produces the snake. `rung` scenario command.
  - **Perfect spiral maze** (`World::add_perfect_spiral`) — a single continuous Archimedean
    spiral wall across all sectors, from a scalar field `f(r,θ) = (R0−r)/pitch + dir·θ/2π`
    whose integer contours are the windings. Walls every grid edge where `floor(f)` changes
    (cardinal AND diagonal, so it's solid) and applies the same test across the seams to join
    the wall over the borders. One smooth spiral corridor winds from the edge to the Nexus.
    `spiral pitch=P dir=cw|ccw` + `goal=nexus`. Tests `perfect_spiral_connects_and_is_long`,
    `perfect_spiral_wall_blocks_radial`. Single-unit trace + radius/path plots published at
    `Active/spiral-trace` (`tools/spiral_trace.py`, `MAD_TRAJ` trajectory dump). Walls only the
    CARDINAL crossing edges (no diagonal "teeth"/comb — the strict diagonal rule blocks slips);
    on the polar grid the wall follows row-arcs/col-radials, so it's a smooth curve.
  - **Bridged borders (seam-solid walls)** — a wall that reaches a seam must *join across the
    border*, or units slip from outside a ring (one sector) to inside it (the neighbour) via the
    seam crossing. Each spiral ring now seals the seam crossings at its radius (via
    `add_boundary_wall`), so the ring is continuous across all three seams; the open bands
    *between* rings keep their crossings for lapping. Test `spiral_bridges_seams`. The general
    primitive is the ALONG wall — sealing a seam crossing at a radius "joins" the coordinate
    systems there.
  - Crossings join cells at the same radius on both sides, so two adjacent players can align
    rungs at the same ring and a demon transitions between wedges without changing radius
    (test `boundary_crossing_preserves_radius`).
- **Demon path trails** — `MAD_TRAILS=1` (or `record.sh --trails`) draws per-demon position
  history; invaluable for seeing what pathing actually does. `rendering::Trails`.
- **Strict no-corner-cutting** (2026-06-15 fix): a size-1 diagonal move is blocked if the
  diagonal edge OR *either* adjacent cardinal edge is walled (previously required *both*),
  so walls are solid barriers and units can't slip diagonally past a single maze wall. The
  size>1 path was already strict; this aligned size-1 with it.
- `core::Recorder` — headless SDL **offscreen** software renderer → one render-target
  texture per camera → `RenderReadPixels` → PPM frames + `manifest.txt`. No display.
- `core::serialize_world` / `deserialize_world` — snapshot/restore dynamic sim state
  (tick, RNG, demon roster). Determinism + serialization primitive for Path C.
- `tools/capture.py` + `record.sh` — frames → per-camera MP4 + 2×2 composite grid →
  HTML report published to `/Content/<timestamp>.1/` and featured under `Active/`.

#### 3.7 Tests
- `test_main.cpp` invokes `mad::test::run_all()`.
- **78 tests passing, 0 failing** as of this writing (was 68; +4 camera, +6 world).
- Existing coverage: grid movement / footprints / corner-cutting; wall edge
  normalization & blocking; sector creation / rotation / world transforms / wedge
  masking / boundary cells; A* one-sector and cross-sector / string-pull / size; flow
  field generation; crystal mid-wedge cell + walkability across N=3..6.
- New coverage: camera center/up-axis/rotation/screen↔world round-trip; flow-field
  manager caching/invalidation; demons move and **complete the full shard route to the
  Nexus**; scenario parsing; serialize round-trip; **simulation determinism** (same
  seed+inputs ⇒ identical demon positions).

### Partially done / stubbed
- `core::Engine::run` — the **windowed** loop still has empty update/render bodies. The
  renderer exists and is fully exercised by the headless recorder; wiring it into the
  windowed `Engine` is a small follow-up (deferred because the container has no display).
- `rendering::Window` — wraps SDL2 window + renderer; the windowed path doesn't draw yet
  (the offscreen recorder path does).
- `network::UDPSocket` — raw blocking-mode UDP send/recv; **no reliability layer, no peer
  discovery**. State **serialization now exists** (`core::serialize_world`), so the netcode
  has a determinism + snapshot foundation to build on.
- **Flow-field manager** — DONE for steering, but does not yet expose "cost-to-goal at a
  boundary" for choosing the cheapest cross-sector route; the handoff currently uses the
  nearest boundary cell, not the cheapest.
- **Tower system** — placement exists (`World::place_tower` stamps a footprint and
  invalidates fields). No upgrade-in-place, no 3→1 merge, no stats/targeting.
- **Crystal** — has a position + is rendered + is a flow-field goal, but is still not a
  `CellState` occupant with HP/shard count/footprint reservation.

### Designed but not implemented at all
*Everything below has design notes in `game_design.md`/`architecture.md` but zero code.*

- **Demon enrage** — none of the 4 candidate strategies. Demons that can't reach a goal
  currently just hold position (the hook is marked in `World::step_demon`). No path-blockage
  attribution, no anti-ping-pong (travel-budget / backtrack-threshold).
- **Wave manager** — spawns are scripted via scenario events; there's no in-game schedule,
  automatic CW/CCW alternation, or portal-edge selection logic.
- **Demon combat/HP** — demons carry `hp` but nothing damages them; towers don't fire.
- **Magic schools, hybrids, specialization progression** — pure design.
- **Projectile system, damage model**.
- **Multiplayer (lobby, sync, peer host)** — serialization exists; transport/reliability does not.
- **Windowed renderer + UI** — the draw code exists; the interactive window/camera/UI wiring does not.
- **Demon tech progression** (era-based wave difficulty).
- **Threading model from architecture.md** — currently single-threaded.

## 4. Coordinate Systems & Conventions

These are easy to get wrong; double-check before changing geometry code.

### World space
- 2D, doubles, units = "world units". `cell_size=1.0` in current `MapConfig`, so 1 world unit ≈ 1 cell.
- **Origin = the Nexus** (map center).
- `+Y` axis points "up" (toward player 0's portal). `+X` axis points right.
- Angles are measured *from `+Y`, clockwise*. This matches how sectors are rotated and how `Sector::contains_world` uses `atan2(pos.x, pos.y)` rather than the more typical `atan2(y, x)`.

### Sector grid = a POLAR wedge (changed 2026-06-17)
Each sector is a wedge of one shared **polar** grid (was a rotated square grid). This makes the
grid **continuous across borders** (a curved line, no kink) and makes rings/spirals render as
smooth curves. `Sector::cell_to_world`:
- **column → angle**: `α = (2π·i/N) − half_angle + (col+0.5)/W · 2·half_angle` (from `+Y`, CW).
  Column 0 = the sector's CCW (left) border, column `W` edge = the CW (right) border.
- **row → radius**: `r = map_radius · (1 − (row+0.5)/H)`. Row 0 = portal (outer, `r≈map_radius`),
  last row = the Nexus (`r≈0`).
- `world = (r·sinα, r·cosα)`. `world_to_cell` reads `r=hypot`, `α=atan2(x,y)` and buckets.
- `cell_corners(cell)` → 4 polar corners (two radial edges, two arc edges). The renderer and
  `mad --coords` use it.
- **No masking**: columns span exactly the wedge, so every cell is inside it. `half_angle = π/N`.
- Adjacent sectors share a border that is exactly a column boundary: `(i, W−1, row)` abuts
  `(i+1, 0, row)` at the **same radius**. `GameMap::boundary_cells` pairs these directly.
- Tradeoff: cells shrink toward the Nexus (narrow near the centre).

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
- **Diagonal moves use a strict, SYMMETRIC solid-corner rule (2026-06-15).** A size-1 diagonal is blocked by its diagonal edge OR by *any* of the four cardinal edges meeting at the corner vertex it crosses. This makes walls solid (no slipping past a single wall) and keeps `can_move(A,B) == can_move(B,A)` — essential because an asymmetric rule lets the flow-field cost flood one way but not steer back, stranding units at wall corners. Tests: `grid_diagonal_can_move_symmetric`, `grid_single_wall_blocks_diagonal_squeeze`. (The size>1 diagonal path is not yet symmetric — revisit for 2x2/3x3 units.)
- **Flow-field direction must re-check passability (2026-06-15 fix).** `FlowField::compute_directions` originally chose the lowest-cost neighbor without a wall check, so the direction field demons steer along pointed *through* walls even though the cost field routed around them — the main "units ignore walls" bug. It now re-runs the same `can_move`/footprint check as generation. Test: `flow_field_direction_never_crosses_wall_edge`.
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

All three paths got a first slice on 2026-06-14 (see §3.6). Remaining work per path:

**Path A — Gameplay loop** (loop exists: demons walk the shard route to the Nexus)
1. **Demon enrage** — implement obstruction-aware smashing + anti-ping-pong; hook at the
   "hold position" branch in `World::step_demon`.
2. **Cost-aware boundary handoff** — choose the cheapest seam using flow-field cost-to-goal
   instead of the nearest boundary cell.
3. In-game **WaveManager** (CW/CCW alternation, portal selection) instead of scripted spawns.
4. Tower targeting/projectiles + demon HP so defense actually does something.
5. Crystal as a real `CellState` occupant with shards/HP/footprint.

**Path B — On screen** (renderer exists; exercised headlessly)
1. Wire `Renderer` into the **windowed** `Engine::update/render` with an interactive camera.
2. Flow-field cost/direction debug overlay; grid lines; HP bars; HUD text (SDL_ttf).
3. Sprite/animation system.

**Path C — Infrastructure** (state serialization done)
1. Threading model from `architecture.md` (logic / pathfinding threads).
2. UDP reliability layer (sequence numbers, ACKs, replay window) on top of `UDPSocket`.
3. Wire `serialize_world` into a host→peer sync + event-replay path (determinism is already
   tested, so lockstep is viable).

The AI harness (`record.sh`) should be the verification path for all of the above: add a
scenario that exercises the feature, record it, review the video.

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

- Branch: `master`. Remote `origin` = `git@github.com:AllenGreen/MAD.git`.
- Commits: `0dd97d2` (scaffolding) → `178f6d0` (crystal positions).
- **Uncommitted** (this session, 2026-06-14): the entire AI harness + first gameplay loop —
  `core/world`, `core/scenario`, `core/recorder`, `core/serialize`, `game/flow_field_manager`,
  `rendering/camera`, `rendering/renderer`, `tools/capture.py`, `record.sh`, `scenarios/`,
  `tests/test_camera.cpp`, `tests/test_world.cpp`, `documentation/ai_harness.md`, plus edits to
  `CMakeLists.txt`, `main.cpp`, this file. Not yet committed (awaiting review).
