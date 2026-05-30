#pragma once

#include <cstdint>
#include <functional>

namespace mad::core {

struct EngineConfig {
    const char* window_title = "Mages Against Demons";
    int window_width = 1280;
    int window_height = 720;
    double tick_rate = 60.0; // Fixed timestep Hz
};

class Engine {
public:
    explicit Engine(const EngineConfig& config = {});
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Run the main loop. Blocks until quit.
    void run();

    // Request shutdown
    void quit();

    bool is_running() const { return running_; }

private:
    void init();
    void shutdown();
    void process_events();
    void update(double dt);
    void render();

    EngineConfig config_;
    bool running_ = false;
    bool initialized_ = false;
};

} // namespace mad::core
