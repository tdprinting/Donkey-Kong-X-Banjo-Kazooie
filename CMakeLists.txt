cmake_minimum_required(VERSION 3.20)

project(JiggyJungle LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(JIGGY_BUILD_TOOLS "Build the jiggy-inspect command-line tool" ON)
option(JIGGY_BUILD_TESTS "Build the bridge tests" ON)

# The host (DK64 Recompiled via N64ModernRuntime) already provides a `miniz` target.
# Standalone builds fetch the same miniz commit the runtime pins.
if (NOT TARGET miniz)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(miniz
        GIT_REPOSITORY https://github.com/richgel999/miniz
        GIT_TAG 8573fd7cd6f49b262a0ccc447f3c6acfc415e556
    )
    FetchContent_MakeAvailable(miniz)
endif()

add_subdirectory(bridge)

if (JIGGY_BUILD_TOOLS)
    add_executable(jiggy-inspect tools/jiggy-inspect/main.cpp)
    target_link_libraries(jiggy-inspect PRIVATE jiggy_bridge)
endif()

if (JIGGY_BUILD_TESTS)
    enable_testing()
    add_executable(jiggy_bridge_tests tests/test_main.cpp)
    target_link_libraries(jiggy_bridge_tests PRIVATE jiggy_bridge miniz)
    add_test(NAME jiggy_bridge_tests COMMAND jiggy_bridge_tests)
endif()
