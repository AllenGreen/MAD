# Mages Against Demons (MAD) - Game Design Document

## Concept

A tower defense game with square-grid maps, multiplayer support, and deep magic specialization.

**Players** are Mages who summon towers of various magic schools to defend the **Heavenly Nexus** (Portal to Heaven) at the center of the map. Each player also has a **Summoning Crystal** in the middle of their sector. Demons must collect shards from every Crystal before they can enter the Nexus. Demons spawn from portals at the map edges.

## Lore

The demons have already conquered the Earthly Realm. They now push through the **Magic Realm** — where time moves slowly — on their march toward Heaven. Along the way they have perverted and adopted human technology: archers, catapults, siege engines, and eventually machine guns and beyond. As the game progresses, demonized human technology advances (Civilization-style tech progression), making waves increasingly difficult.

## Map Geometry

- **2D square grid** per player sector, used for both placement and pathing.
- Map shape is an N-sided polygon determined by the number of human players:
  - 2 players → Rectangle
  - 3 players → Triangle
  - 4 players → Square
  - 5 players → Pentagon
  - 6 players → Hexagon
  - etc.
- Each edge of the polygon is a **Demon Portal**.
- The **Heavenly Nexus** sits at the center.
- Portals may spawn units on 1 to N edges per wave (where N = number of edges).

### Sector Grid System

Each player owns a **wedge-shaped sector** from their polygon edge to the Nexus center. The map is divided into N equal sectors.

- **Per-player square grid**: Each player has a square grid aligned so their edge is "at the top." Towers, walls, and pathing all operate on this grid.
- **Player perspective**: Every player sees themselves at the top of the screen. Other players' sectors are rotated and rendered at their actual angle.
- **Boundary slices**: The angled lines between two adjacent players' sectors are **special-case zones**. Walls can be placed exactly on a shared boundary slice. Custom logic handles:
  - Wall creation on boundary edges (must be valid from both players' perspectives)
  - Pathfinding across sector boundaries (units transitioning between two rotated grids)
  - Visual stitching so rotated grids appear seamless
- **Path smoothing**: String-pulling (funnel algorithm) applied to grid paths so units walk in straight lines and turn at actual corners rather than zig-zagging on grid edges.

### Summoning Crystals and the Shard Mechanic

Each player has a **Summoning Crystal** positioned at the center of their wedge, roughly midway between the portal edge and the Nexus. Demons must collect a **shard** from every Crystal on the map before they can enter the Nexus.

**Shard collection order**: Demons visit Crystals in rotational order. Waves alternate between clockwise (CW) and counter-clockwise (CCW) direction each wave.

**Example (3-player CW wave, spawning at Player 2's edge)**:

1. Demons spawn at Player 2's portal edge
2. Path to Player 2's Crystal (collect shard 1)
3. Cross boundary into Player 0's sector, path to Player 0's Crystal (shard 2, CW)
4. Cross boundary into Player 1's sector, path to Player 1's Crystal (shard 3)
5. Path to the Nexus at the center (all shards collected, enter Heaven)

This mechanic forces **all players to collaborate** defensively — demons traverse every sector, so no player can sit idle.

### Flow Fields for Shard Pathing

Demons follow flow fields based on their current goal. There are **N + 1 flow fields** per (unit_size, move_type) combination:
- 1 flow field per Crystal (goal = that Crystal's position)
- 1 flow field for the Nexus (goal = Nexus cells)

A demon tracks its `shards_collected` count and `wave_direction` (CW/CCW). It follows the flow field for its current goal Crystal, and switches to the next Crystal's field upon shard collection. After all shards, it switches to the Nexus field.

Flow fields are per-sector (Option B architecture). When a demon reaches a sector boundary, it transitions to the neighbor sector's flow field. Boundary transitions include cost-to-goal information so demons choose the shortest path across boundaries.

### Demon Enrage (Blocked Pathing)

When a demon's path to its current goal is completely blocked, it enters an **Enrage** state and begins smashing structures. Prospective approaches for choosing what to smash:

1. **Smash most recent obstruction**: Track which player construction most recently blocked the path. Smash that structure. Fairest to the player — punishes the wall that actually caused the block.
2. **Path toward boundary, then smash**: Path as close to the sector boundary (toward the goal sector) as possible, then start smashing in the goal direction.
3. **Smash nearest**: Simply target the nearest structure. Simplest but least strategic.
4. **Hybrid**: Try approach 1 first; fall back to 2 if the offending structure can't be determined.

**Anti-ping-pong measures**: Demons must not endlessly traverse the same path without making progress. Two candidate mechanisms:
- **Max travel budget**: Each demon has a travel distance limit per shard goal. If exceeded without collecting the shard, it enrages.
- **Backtrack detection**: Track cumulative distance traveled in the "wrong" direction (away from goal). If backtracking exceeds a threshold, enrage.

The exact enrage behavior will be refined during gameplay testing. The pathing system must support:
- Querying whether a goal is reachable (flow field cost = UNREACHABLE)
- Tracking per-demon travel distance and backtracking
- Identifying which structure blocked a previously valid path (requires path invalidation events to record the causing placement)

## Unit Types (Demons)

| Type     | Behavior                                              |
|----------|-------------------------------------------------------|
| Ground   | Paths along grid; blocked by walls. Sizes: 1x1, 2x2, 3x3 |
| Climber  | Can scale short walls and buildings. Sizes: 1x1, 2x2      |
| Flyer    | Ignores terrain; flies over walls. Sizes: 1x1, 2x2        |
| Smasher  | Destroys walls/buildings in a straight line. Sizes: 1x1, 2x2, 3x3 |

Unit sizes: 1x1 (normal), 2x2 (large/mini-boss), 3x3 (boss). Larger units must take wider paths — players can build narrow 1-wide corridors that small units pass through while forcing 2x2+ onto longer routes.

## Towers

- Summoned by Mages onto grid tiles.
- Sizes: 1x1, 3x3, and larger.
- **Upgrade paths**: towers can be upgraded in place.
- **Merge paths**: three 1x1 towers of compatible types can merge into a more powerful tower.

## Magic Schools

| School             | Theme                        |
|--------------------|------------------------------|
| Light              | Holy damage, healing, buffs  |
| Darkness           | Curses, debuffs, DoT         |
| Nature             | Entangle, poison, terrain    |
| Summoning/Abjuration | Summon allies, banish enemies |
| Alteration         | Transform, polymorph, slow   |
| Fire               | AoE burn, burst damage       |
| Ice                | Slow, freeze, shatter        |
| Void               | Gravity, displacement, deletion |
| Crystal            | Reflect, refract, pierce     |
| Necromancy         | Raise dead units, life drain |
| Mechanomancy       | Machines + Magic hybrids     |

### Opposites and Hybrids

Schools have opposites (e.g., Light/Darkness, Fire/Ice, Nature/Mechanomancy). Hybrid towers combine two compatible schools for unique effects.

## Specialization Progression

| Game Phase | Specialization Options                                     |
|------------|-----------------------------------------------------------|
| Early      | 1 specialty OR generalist                                  |
| Mid        | 2 specialties OR 1 specialty + hybrid-generalist           |
| Late       | 3 specialties                                              |

Players are encouraged to specialize and coordinate with teammates.

## Multiplayer

- Network server initially hosted on a player's PC (peer-hosted).
- Future: dedicated cloud servers.
- Each player defends their sector of the map (edges nearest to them).
- Players can assist neighbors and coordinate specializations.

## Demon Tech Progression

Waves advance through eras of perverted human technology:

1. **Primitive**: clubs, thrown rocks
2. **Ancient**: demonized archers, spearmen
3. **Medieval**: siege towers, catapults, armored knights
4. **Renaissance**: early cannons, sappers
5. **Industrial**: machine guns, artillery
6. **Modern**: tanks, aircraft
7. **Future**: energy weapons, mechs
8. **Apocalyptic**: reality-warping demon-tech fusions

## Critical Systems (Performance)

- **Pathfinding**: Square-grid A* with flow fields for mass units. String-pulling for smooth movement. Must handle dynamic wall placement, path invalidation, and cross-sector boundary transitions efficiently.
- **Projectile tracking**: Predictive targeting, splash damage calculations, many simultaneous projectiles.
- **Multithreading**: Separate threads for rendering, game logic, network, and pathfinding.
- **Network**: Lean, reliable UDP-based core with game-state synchronization.
