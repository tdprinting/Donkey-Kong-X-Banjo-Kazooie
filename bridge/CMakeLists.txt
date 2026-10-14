add_library(jiggy_bridge STATIC
    src/bk_assets.cpp
    src/bk_sprite.cpp
    src/bridge.cpp
    src/dk64_tables.cpp
    src/inflate.cpp
    src/rom.cpp
    src/rom_import.cpp
)

target_include_directories(jiggy_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(jiggy_bridge PRIVATE miniz)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(jiggy_bridge PRIVATE -Wall -Wextra -Wpedantic)
endif()
