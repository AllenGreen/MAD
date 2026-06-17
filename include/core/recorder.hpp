#pragma once

#include "core/scenario.hpp"

#include <string>

namespace mad::core {

// Runs a Scenario headlessly and records every camera to a sequence of PPM
// frames plus a manifest the capture tool turns into video. No window is shown;
// SDL uses the "offscreen" video driver and a software render target, so this
// works on a server with no display. Fully deterministic.
struct RecordResult {
    bool ok = false;
    std::string error;
    uint64_t frames = 0;
    int demons_reached_nexus = 0;
};

// `out_dir` receives: <out_dir>/<camera_name>/frame_000001.ppm ... and
// <out_dir>/manifest.txt. Existing contents are reused/overwritten.
RecordResult record_scenario(const Scenario& scenario, const std::string& out_dir);

} // namespace mad::core
