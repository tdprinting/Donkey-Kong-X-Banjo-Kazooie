add_library(jiggy_updater STATIC src/install.cpp src/updater.cpp)
target_include_directories(jiggy_updater PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# The bridge provides gzip inflation for .tar.gz releases; miniz reads .zip releases.
target_link_libraries(jiggy_updater PRIVATE jiggy_bridge miniz)

# nlohmann/json, included as "json/json.hpp". The host passes the copy N64ModernRuntime
# bundles; standalone builds download the same release (v3.12.0), checked by hash.
if (NOT JIGGY_JSON_INCLUDE_DIR)
    set(JIGGY_JSON_INCLUDE_DIR ${CMAKE_BINARY_DIR}/_deps/nlohmann_json)
    file(DOWNLOAD
        https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp
        ${JIGGY_JSON_INCLUDE_DIR}/json/json.hpp
        EXPECTED_HASH SHA256=aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63
    )
endif()
target_include_directories(jiggy_updater PRIVATE ${JIGGY_JSON_INCLUDE_DIR})

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(jiggy_updater PRIVATE -Wall -Wextra)
endif()
