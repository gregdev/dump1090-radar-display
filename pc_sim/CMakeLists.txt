cmake_minimum_required(VERSION 3.16)
project(radar_sim LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ── LVGL ──────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.2.0
)
# Tell LVGL where to find our lv_conf.h
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
FetchContent_MakeAvailable(lvgl)

# Exclude demos, examples, and thorvg from the build (we only need lvgl)
set_target_properties(lvgl_demos    PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(lvgl_examples PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(lvgl_thorvg   PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Ensure LVGL can find our lv_conf.h (belt-and-suspenders)
target_include_directories(lvgl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ── SDL2 ──────────────────────────────────────────────────
find_package(SDL2 REQUIRED)

# ── Our source files ──────────────────────────────────────
set(RADAR_SRC ../src)

add_executable(radar_sim
    main.c
    ${RADAR_SRC}/radar_ui.cpp
    ${RADAR_SRC}/aircraft_data.cpp
    ${RADAR_SRC}/coord_convert.cpp
)

target_include_directories(radar_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/${RADAR_SRC}
)

target_compile_definitions(radar_sim PRIVATE PC_SIMULATOR)

target_link_libraries(radar_sim PRIVATE
    lvgl
    SDL2::SDL2
)

# ws2_32 is Windows-only
if(WIN32)
    target_link_libraries(radar_sim PRIVATE ws2_32)
endif()
