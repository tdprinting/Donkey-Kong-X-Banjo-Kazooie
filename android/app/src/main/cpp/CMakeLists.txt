cmake_minimum_required(VERSION 3.20)
project(JiggyJungleAndroid LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The Jiggy Bridge and updater from the repository root; the tools and tests aren't needed.
set(JIGGY_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(JIGGY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../.. jiggy)

add_library(jiggyjungle SHARED jiggy_jni.cpp)
target_link_libraries(jiggyjungle PRIVATE jiggy_bridge jiggy_updater log)
