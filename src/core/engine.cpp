#include "core/engine.hpp"
#include "core/log.hpp"
#include "rendering/window.hpp"

#include <SDL2/SDL.h>
#include <chrono>
#include <memory>
#include <thread>

namespace mad::core {

static constexpr const char* TAG = "Engine";

static std::unique_ptr<rendering::Window> s_window;

Engine::Engine(const EngineConfig& config)
    : config_(config) {}

Engine::~Engine() {
    shutdown();
}

void Engine::init() {
    if (initialized_) return;

    log::info(TAG, "Initializing MAD engine v0.1.0");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        log::error(TAG, "SDL_Init failed: {}", SDL_GetError());
        return;
    }

    s_window = std::make_unique<rendering::Window>(
        config_.window_title,
        config_.window_width,
        config_.window_height
    );

    initialized_ = true;
    log::info(TAG, "Engine initialized");
}

void Engine::shutdown() {
    if (!initialized_) return;

    log::info(TAG, "Shutting down");
    s_window.reset();
    SDL_Quit();
    initialized_ = false;
}

void Engine::run() {
    init();
    if (!initialized_) return;

    running_ = true;
    const double dt = 1.0 / config_.tick_rate;
    auto previous = std::chrono::steady_clock::now();
    double accumulator = 0.0;

    while (running_) {
        auto current = std::chrono::steady_clock::now();
        double frame_time = std::chrono::duration<double>(current - previous).count();
        previous = current;

        // Clamp to avoid spiral of death
        if (frame_time > 0.25) frame_time = 0.25;
        accumulator += frame_time;

        process_events();

        while (accumulator >= dt) {
            update(dt);
            accumulator -= dt;
        }

        render();
    }

    shutdown();
}

void Engine::quit() {
    running_ = false;
}

void Engine::process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                quit();
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    quit();
                break;
        }
    }
}

void Engine::update([[maybe_unused]] double dt) {
    // Game logic will go here
}

void Engine::render() {
    if (!s_window) return;
    s_window->clear();

    // Rendering will go here

    s_window->present();
}

} // namespace mad::core
