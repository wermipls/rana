cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(corn C CXX)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    # Git auto-ignore out-of-source build directory
    file(GENERATE OUTPUT .gitignore CONTENT "*")
endif()

set(CMAKE_CXX_STANDARD 20)

find_package(pugixml)

set(BUILD_SHARED_LIBS OFF)

# Your application target.
add_executable(corn WIN32
    corn.cpp
    ../miniz/miniz.c
)

target_link_libraries(corn -static -fdiagnostics-color=always)
target_compile_options(corn PRIVATE -ggdb -fdiagnostics-color=always)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_libraries(corn -fansi-escape-codes)
    target_compile_options(corn PRIVATE -fansi-escape-codes)
endif()

if(MSVC)
  target_compile_options(corn PRIVATE /W4)
else()
  target_compile_options(corn PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(corn pugixml::pugixml)

