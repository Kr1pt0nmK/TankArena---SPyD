cmake_minimum_required(VERSION 3.16)
project(tank_arena C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3  REQUIRED gtk+-3.0)
pkg_check_modules(CAIRO REQUIRED cairo)

# --- Juego: ventana GTK interactiva ---
add_executable(tank-arena src/main.c src/render.c src/game.c)
target_include_directories(tank-arena PRIVATE ${GTK3_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)
target_link_directories(tank-arena PRIVATE ${GTK3_LIBRARY_DIRS})
target_link_libraries(tank-arena PRIVATE ${GTK3_LIBRARIES})
target_compile_options(tank-arena PRIVATE ${GTK3_CFLAGS_OTHER})
target_compile_definitions(tank-arena PRIVATE
    UI_FILE="${CMAKE_SOURCE_DIR}/ui/tankarena.glade")

# --- Preview: renderiza un frame a PNG, sin ventana ---
add_executable(preview src/preview.c src/render.c src/game.c)
target_include_directories(preview PRIVATE ${CAIRO_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)
target_link_directories(preview PRIVATE ${CAIRO_LIBRARY_DIRS})
target_link_libraries(preview PRIVATE ${CAIRO_LIBRARIES})
target_compile_options(preview PRIVATE ${CAIRO_CFLAGS_OTHER})

if(NOT WIN32)
    target_link_libraries(tank-arena PRIVATE m)
    target_link_libraries(preview    PRIVATE m)
endif()
