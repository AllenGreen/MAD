# MAD — AI Development Harness

MAD is built to be developed **AI-first**: an agent can author a scenario, run
the game headlessly (no display), record it from several cameras, and publish a
video report a human reviews in the IdeaBox web console — all from the command
line. This document is the harness reference.

## The loop

```
scenario (.mad)  ->  ./record.sh  ->  headless sim + multi-camera capture
                                   ->  ffmpeg encode (per-camera + 2x2 grid)
                                   ->  /Content/<timestamp>.1/index.html
                                   ->  featured under /Content/Active/
```

One command:

```bash
./record.sh scenarios/demo_3p.mad
```

This builds Release, runs the scenario, encodes every camera to MP4, builds a
composite grid, and publishes an HTML page (with posters, a demon-type legend,
and run stats) to `/Content`. The human sees it as a tab in the console.

Useful flags (forwarded to `tools/capture.py`):
- `--no-build` — reuse the current `build/` binary.
- `--no-publish` — record + encode into `/Data` but don't touch `/Content`.
- `--frames-dir DIR` — override the PPM scratch dir (default `/Data/mad_capture/<name>`).

### Lower-level entry points

- **Record only** (frames + manifest, no video):
  ```bash
  ./build/mad --record scenarios/demo_3p.mad --out /Data/mad_capture/demo_3p
  ```
  Frames land in `<out>/<camera>/frame_000001.ppm`; a `manifest.txt` lists the
  cameras, fps, frame count, and how many demons reached the Nexus.
- **Encode/publish existing frames**: `tools/capture.py <scenario> --no-build`
  re-runs the recorder then encodes; pass `--no-publish` to keep it local.
- Set `MAD_LOG_DEBUG=1` to restore verbose per-demon logging during a record.

Everything is deterministic: same scenario + seed ⇒ identical frames.

## Scenario grammar

A scenario is a line-based text file (`#` starts a comment). Order is free; events
are sorted by tick at parse time. All keys are `key=value`.

| Line | Meaning |
|---|---|
| `name <text...>` | Title shown in the report. |
| `map players=N grid_w=W grid_h=H radius=R cell=C` | Map dimensions. |
| `seed <uint>` | RNG seed (determinism). |
| `tick_rate <hz>` | Simulation step rate (default 60). |
| `fps <hz>` | Capture frame rate (default 30). |
| `ticks <n>` | How many ticks to simulate. |
| `camera overview [name=.. w=.. h=..]` | Top-down whole-map camera. |
| `camera sector id=I [name=.. w=.. h=..]` | Player I's perspective (portal up). |
| `at <tick> spawn sector=S type=ground\|climber\|flyer\|smasher size=1\|2\|3 count=K dir=cw\|ccw` | Spawn a wave at S's portal. |
| `at <tick> wall sector=S col=C row=R edge=horizontal\|vertical\|diagne\|diagnw [height=1\|2]` | Place a wall edge. |
| `at <tick> tower sector=S col=C row=R size=K` | Place a size×size tower footprint. |
| `at <tick> maze sector=S [height=1\|2] [seed=N]` | Fill the sector with a random perfect maze (seed derived from the scenario seed if omitted). Stays solvable by construction. |
| `at <tick> spawn ... [goal=nexus]` | `goal=nexus` makes the wave skip the shard route and head straight for the Nexus (used for clean spiral demos). |
| `at <tick> border sector=S neighbor=T row=R [count=K] [height=2]` | ALONG wall: seal K seam crossings on the S/T boundary at radius (ring) R outward (R is cells from the nexus). Whole-map pathing reroutes to an open radius. |
| `at <tick> rung sector=S row=R side=left\|right [gap=3] [height=2]` | PERP rung: a tangential wall across sector S at grid `row` R, gap at the `side` border. Alternating gaps build a serpentine. |
| `at <tick> spiral pitch=P [dir=cw\|ccw] [height=2]` | A single continuous Archimedean spiral wall across ALL sectors (radius drops `P` per turn), leaving one smooth spiral corridor from the edge to the Nexus. See `scenarios/spiral.mad`; `tools/spiral_trace.py` traces+plots one unit. |

If no `camera` line is given, a single overview camera is used.

### Sizing rule (important)

The wedge geometry constrains map dimensions. For N players:
- `grid_height ≈ map_radius` so the grid actually reaches the Nexus (otherwise the
  innermost rows fall short and demons can't finish).
- `grid_width ≥ 2·sin(π/N)·map_radius` so the shared boundary line between
  adjacent sectors passes through both grids (otherwise cross-sector hops have no
  boundary cells and demons stall after their first crystal).

For 3 players that's `grid_w ≳ 1.73·R`; for 4 players `≳ 1.41·R`. See
`scenarios/demo_3p.mad` for a worked example.

## What the cameras show

- **overview** — unrotated, Nexus centered, all sectors visible.
- **sector i** — rotated so player i's portal is at the top and the Nexus at the
  bottom: "every player sees themselves at the top."

Demon colors: Ground = red, Climber = orange, Flyer = light blue, Smasher =
purple. Crystals are cyan diamonds; the Nexus is the gold core; walls are tan
(bright = tall, dim = short); towers are tan blocks.

## How it works under the hood

- `core::World` — the deterministic, render-free simulation (map, flow-field
  manager, demons, wave spawns). Fixed timestep.
- `game::FlowFieldManager` — lazily builds/caches the N+1 fields per
  `(MoveType, size)`: one per crystal, one per Nexus, plus boundary fields used
  for cross-sector handoff. Invalidated on wall/tower placement.
- `rendering::Camera` / `rendering::Renderer` — project the World to any
  `SDL_Renderer`; used identically by the (future) window and the recorder.
- `core::Recorder` — SDL "offscreen" software renderer → render-target texture
  per camera → `SDL_RenderReadPixels` → PPM frames + manifest. No display.
- `core::serialize_world` — snapshot/restore dynamic sim state (tick, RNG,
  demons); the determinism + serialization primitive Path C (netcode) builds on.
- `tools/capture.py` — frames → MP4 (libx264/yuv420p) + composite grid → HTML
  report published to `/Content` and featured under `Active/`.
- `mad --coords <file>` + `tools/coords_page.py` — emit the engine's exact
  sector/seam geometry and render a published reference page (zoomed SVG diagrams
  at three radii) explaining the rotated-grid → world transform and how cells are
  paired across the border. Featured at `Active/coordinate-system`.
