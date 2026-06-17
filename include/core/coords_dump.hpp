#pragma once

#include <string>

namespace mad::core {

// Write the engine's exact sector/seam geometry (for the demo map) to a text
// file used by tools/coords_page.py. Returns false if the file can't be opened.
bool dump_coords(const std::string& path);

} // namespace mad::core
