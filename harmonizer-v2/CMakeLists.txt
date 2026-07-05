cmake_minimum_required(VERSION 3.16)

project(harmonizer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

add_executable(harmonizer
    src/harmonizer.c
)

target_include_directories(harmonizer PRIVATE
    ${PIPEWIRE_INCLUDE_DIRS}
)

target_compile_options(harmonizer PRIVATE
    ${PIPEWIRE_CFLAGS_OTHER}
    -Wall
    -Wextra
    -Wpedantic
)

target_link_directories(harmonizer PRIVATE
    ${PIPEWIRE_LIBRARY_DIRS}
)

target_link_libraries(harmonizer
    ${PIPEWIRE_LIBRARIES}
)