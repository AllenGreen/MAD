#include "core/engine.hpp"
#include "core/log.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    mad::core::log::info("Main", "Mages Against Demons v0.1.0");

    mad::core::EngineConfig config;
    config.window_title = "Mages Against Demons";
    config.window_width = 1280;
    config.window_height = 720;

    mad::core::Engine engine(config);
    engine.run();

    return 0;
}
