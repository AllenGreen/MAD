#include "rendering/window.hpp"
#include "core/log.hpp"

namespace mad::rendering {

static constexpr const char* TAG = "Window";

Window::Window(std::string_view title, int width, int height)
    : width_(width), height_(height)
{
    window_ = SDL_CreateWindow(
        std::string(title).c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window_) {
        core::log::error(TAG, "Failed to create window: {}", SDL_GetError());
        return;
    }

    renderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer_) {
        core::log::error(TAG, "Failed to create renderer: {}", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return;
    }

    core::log::info(TAG, "Created {}x{} window", width, height);
}

Window::~Window() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
}

void Window::clear() {
    SDL_SetRenderDrawColor(renderer_, 15, 10, 25, 255); // Dark purple-black
    SDL_RenderClear(renderer_);
}

void Window::present() {
    SDL_RenderPresent(renderer_);
}

} // namespace mad::rendering
