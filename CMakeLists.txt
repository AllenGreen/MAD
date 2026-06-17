cmake_minimum_required(VERSION 3.22)
project(MAD
    VERSION 0.1.0
    DESCRIPTION "Mages Against Demons - Tower Defense"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type defaults to Debug
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Compiler warnings
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -Werror=return-type
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -DNDEBUG -march=native)
    add_link_options(-flto)
endif()

# --- Dependencies ---

# SDL2
find_package(SDL2 REQUIRED)

# Threads
find_package(Threads REQUIRED)

# --- Engine Library ---

add_library(mad_engine STATIC
    src/core/engine.cpp
    src/core/log.cpp
    src/core/world.cpp
    src/core/scenario.cpp
    src/core/recorder.cpp
    src/core/serialize.cpp
    src/core/coords_dump.cpp
    src/rendering/window.cpp
    src/rendering/renderer.cpp
    src/network/socket.cpp
    src/game/grid.cpp
    src/game/wall.cpp
    src/game/sector.cpp
    src/game/game_map.cpp
    src/game/pathfinding.cpp
    src/game/flow_field.cpp
    src/game/flow_field_manager.cpp
    src/game/global_flow_field.cpp
)

target_include_directories(mad_engine PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(mad_engine PUBLIC
    ${SDL2_LIBRARIES}
    Threads::Threads
)

# --- Main Executable ---

add_executable(mad src/main.cpp)
target_link_libraries(mad PRIVATE mad_engine)

# --- Tests ---

option(MAD_BUILD_TESTS "Build tests" ON)
if(MAD_BUILD_TESTS)
    enable_testing()
    add_executable(mad_tests
        tests/test_main.cpp
        tests/test_grid.cpp
        tests/test_wall.cpp
        tests/test_sector.cpp
        tests/test_pathfinding.cpp
        tests/test_camera.cpp
        tests/test_world.cpp
    )
    target_include_directories(mad_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(mad_tests PRIVATE mad_engine)
    add_test(NAME mad_tests COMMAND mad_tests)
endif()
