cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

add_subdirectory(corn)

project(rana CXX)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    # Git auto-ignore out-of-source build directory
    file(GENERATE OUTPUT .gitignore CONTENT "*")
endif()

set(CMAKE_CXX_STANDARD 20)

#set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")

find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA REQUIRED luajit)

set(BUILD_SHARED_LIBS OFF)


# Your application target.
add_executable(rana WIN32
    src/main.cpp
    src/gfx.cpp
)

target_include_directories(rana PRIVATE external)
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

# SDL2::SDL2main may or may not be available. It is e.g. required by Windows GUI applications
if(TARGET SDL2::SDL2main)
    # It has an implicit dependency on SDL2 functions, so it MUST be added before SDL2::SDL2 (or SDL2::SDL2-static)
    target_link_libraries(rana SDL2::SDL2main)
endif()
target_link_libraries(rana SDL2::SDL2-static)


target_link_libraries(rana ${LUA_LIBRARIES})
target_include_directories(rana PUBLIC ${LUA_INCLUDE_DIRS})
target_compile_options(rana PUBLIC ${LUA_CFLAGS_OTHER})

