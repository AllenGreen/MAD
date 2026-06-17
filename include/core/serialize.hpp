#pragma once

#include "core/world.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace mad::core {

// Snapshot/restore the *dynamic* simulation state of a World: tick counter, RNG
// state, demon roster, and run counters. This is the serialization primitive
// Path C (netcode) builds on -- a host can snapshot, ship the bytes, and a peer
// can restore an identical sim. Structural map state (walls/towers) is
// event-sourced (replay the scenario events), so it is not duplicated here.
//
// serialize_world / deserialize_world are declared in core/world.hpp (they are
// friends of World). This header just documents and groups them.
using mad::core::deserialize_world;
using mad::core::serialize_world;

} // namespace mad::core
