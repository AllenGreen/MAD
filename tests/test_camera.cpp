#include "core/test.hpp"
#include "rendering/camera.hpp"

#include <cmath>

using namespace mad;

TEST(camera_screen_center_is_camera_center) {
    rendering::Camera cam;
    cam.center = {3.0, -2.0};
    cam.viewport_w = 800;
    cam.viewport_h = 600;
    auto s = cam.world_to_screen(cam.center);
    ASSERT_TRUE(std::abs(s.x - 400.0f) < 1e-3f);
    ASSERT_TRUE(std::abs(s.y - 300.0f) < 1e-3f);
    return true;
}

TEST(camera_y_axis_points_up) {
    rendering::Camera cam;
    cam.viewport_w = 400;
    cam.viewport_h = 400;
    cam.pixels_per_unit = 10.0;
    // A point above center in world space should be above center on screen
    // (smaller y), because screen Y is flipped.
    auto up = cam.world_to_screen({0.0, 1.0});
    ASSERT_TRUE(up.y < 200.0f);
    return true;
}

TEST(camera_roundtrip_screen_world) {
    rendering::Camera cam;
    cam.center = {5.0, 7.0};
    cam.angle = 0.9;
    cam.pixels_per_unit = 12.5;
    cam.viewport_w = 640;
    cam.viewport_h = 480;
    game::WorldPos p{2.5, -4.0};
    auto s = cam.world_to_screen(p);
    auto back = cam.screen_to_world(s.x, s.y);
    ASSERT_TRUE(std::abs(back.x - p.x) < 1e-6);
    ASSERT_TRUE(std::abs(back.y - p.y) < 1e-6);
    return true;
}

TEST(camera_rotation_puts_sector_line_up) {
    // With angle = sector rotation, a point along that sector's center line
    // (angle measured from +Y, clockwise) should map straight up from center.
    const double rot = 2.0 * M_PI / 3.0; // sector 1 of 3
    rendering::Camera cam;
    cam.angle = rot;
    cam.pixels_per_unit = 10.0;
    cam.viewport_w = 400;
    cam.viewport_h = 400;
    game::WorldPos on_line{std::sin(rot) * 5.0, std::cos(rot) * 5.0};
    auto s = cam.world_to_screen(on_line);
    ASSERT_TRUE(std::abs(s.x - 200.0f) < 1e-2f); // centered horizontally
    ASSERT_TRUE(s.y < 200.0f);                    // above center
    return true;
}
