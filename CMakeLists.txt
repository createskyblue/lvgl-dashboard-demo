cmake_minimum_required(VERSION 3.16)
project(lvgl_sim C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LVGL_SIM_AXIS_OVERLAY 0 CACHE STRING "0: side axis column, 1: labels overlaid on full-width chart")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_compile_definitions(LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")

set(LV_BUILD_EXAMPLES OFF CACHE BOOL "build examples" FORCE)

add_subdirectory(lvgl)

find_package(SDL2 REQUIRED)

add_executable(lvgl_sim
    src/main.c
    src/ui.c
    src/png.c
)
target_include_directories(lvgl_sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(lvgl_sim PRIVATE LVGL_SIM_AXIS_OVERLAY=${LVGL_SIM_AXIS_OVERLAY})
target_link_libraries(lvgl_sim PRIVATE lvgl SDL2::SDL2 z m stdc++)