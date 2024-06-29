cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

add_subdirectory(corn)

project(rana C CXX)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    # Git auto-ignore out-of-source build directory
    file(GENERATE OUTPUT .gitignore CONTENT "*")
endif()

set(CMAKE_CXX_STANDARD 20)

#set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")

add_subdirectory(SDL EXCLUDE_FROM_ALL)

set(BUILD_SHARED_LIBS OFF)

if (NOT TARGET opus)
    add_subdirectory(opus opus)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA REQUIRED luajit)

# Your application target.
add_executable(rana WIN32
    src/main.cpp
    src/gfx.cpp
    src/sdl_error.cpp
    src/audio/freeverb/freeverb.c
)

target_include_directories(rana PRIVATE external)
target_include_directories(rana PRIVATE src)
target_link_libraries(rana -static -static-libgcc -static-libstdc++ -flto -fdiagnostics-color=always)
target_compile_options(rana PRIVATE -ggdb -Ofast -fdiagnostics-color=always)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_libraries(rana -fansi-escape-codes)
    target_compile_options(rana PRIVATE -fansi-escape-codes)
endif()

if(MSVC)
  target_compile_options(rana PRIVATE /W4)
else()
  target_compile_options(rana PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(rana SDL3::SDL3)


target_link_libraries(rana ${LUA_LIBRARIES})
target_include_directories(rana PUBLIC ${LUA_INCLUDE_DIRS})
target_compile_options(rana PUBLIC ${LUA_CFLAGS_OTHER})


target_link_libraries(rana opus)
