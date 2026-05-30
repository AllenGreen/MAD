# MAD - Technical Architecture

## Technology Stack

- **Language**: Modern C++ (C++23)
- **Build**: CMake + Ninja
- **Graphics**: SDL2 (+ SDL2_image, SDL2_ttf, SDL2_mixer)
- **Target**: Steam (via Steamworks SDK, later)
- **Networking**: Custom UDP with reliability layer

## Project Structure

```
MAD/
├── CMakeLists.txt          # Root build configuration
├── build.sh                # Build helper
├── test.sh                 # Test helper
├── run.sh                  # Run helper
├── cmake/                  # CMake modules and find scripts
├── documentation/          # Design docs, architecture
├── src/
│   ├── core/               # Engine fundamentals (loop, time, memory, threading)
│   ├── game/               # Game logic (towers, units, waves, hex grid)
│   ├── network/            # Networking (UDP, sync, lobby)
│   └── rendering/          # SDL rendering, UI, effects
├── include/
│   ├── core/
│   ├── game/
│   ├── network/
│   └── rendering/
├── tests/                  # Unit and integration tests
└── assets/                 # Sprites, sounds, maps (later)
```

## Module Breakdown

### Core (`src/core/`, `include/core/`)
- Game loop (fixed timestep)
- Thread pool / job system
- Memory management (arena allocators, pooling)
- Logging
- Configuration

### Game (`src/game/`, `include/game/`)
- Per-player square grid and coordinate system
- Sector management (N-sided polygon, wedge decomposition)
- Summoning Crystals (one per sector, mid-wedge)
- Boundary slice logic (cross-sector walls, pathing transitions, boundary cost queries)
- Pathfinding (A* per sector, flow fields, string-pulling for smooth movement)
  - N+1 flow fields per (unit_size, move_type): one per Crystal + one for Nexus
  - Multi-size support: 1x1, 2x2, 3x3 footprints
  - Cross-sector boundary handoff with cost-to-goal at transitions
- Shard collection system (demon tracks shards, switches flow field per goal)
- Wave direction management (alternating CW/CCW)
- Demon enrage system (blocked pathing → structure smashing)
  - Path blockage detection (flow field UNREACHABLE)
  - Backtrack/travel budget tracking for anti-ping-pong
  - Obstruction identification (which placement blocked the path)
- Tower system (placement, upgrade, merge)
- Unit system (movement, types, sizes, abilities)
- Wave spawning and management
- Magic school system
- Damage and projectile system

### Network (`src/network/`, `include/network/`)
- UDP socket abstraction
- Reliable message layer
- Game state serialization
- Client/server architecture
- Lobby and matchmaking

### Rendering (`src/rendering/`, `include/rendering/`)
- SDL2 window and renderer management
- Grid rendering (per-sector with rotation)
- Sprite/animation system
- UI system
- Camera and viewport
- Particle effects

## Threading Model

| Thread         | Responsibility                          |
|----------------|----------------------------------------|
| Main           | SDL event loop, rendering              |
| Game Logic     | Tick updates, wave management          |
| Pathfinding    | Path calculations, flow field updates  |
| Network        | Send/receive, serialization            |
| Audio          | Sound mixing (SDL handles internally)  |

## Performance Priorities

1. Pathfinding must handle hundreds of units with dynamic obstacles and cross-sector transitions
2. Projectile system must efficiently track many simultaneous projectiles
3. Network must be low-latency with minimal bandwidth
4. Rendering must maintain 60fps with many units and effects on screen
