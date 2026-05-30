#pragma once

#include <SDL2/SDL.h>
#include <string_view>

namespace mad::rendering {

class Window {
public:
    Window(std::string_view title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Renderer* renderer() const { return renderer_; }
    SDL_Window* handle() const { return window_; }

    void clear();
    void present();

    int width() const { return width_; }
    int height() const { return height_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    int width_;
    int height_;
};

} // namespace mad::rendering
