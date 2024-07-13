cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(rana C CXX)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    # Git auto-ignore out-of-source build directory
    file(GENERATE OUTPUT .gitignore CONTENT "*")
endif()

set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)

add_executable(rana WIN32
    src/main.cpp
    src/fs.cpp
    src/gfx.cpp
    src/sdl_error.cpp
    src/audio/samples.cpp
    src/audio/freeverb/freeverb.c
    src/glad.c
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/backends/imgui_impl_sdl3.cpp
)

# handle dependencies
# SDL3
option(SDL_SHARED "" OFF)
option(SDL_STATIC "" ON)
add_subdirectory(SDL EXCLUDE_FROM_ALL)
target_link_libraries(rana SDL3::SDL3)

# libopus
if (NOT TARGET opus)
    add_subdirectory(opus opus)
endif()
target_link_libraries(rana opus)

# PhysicsFS
option(PHYSFS_ARCHIVE_ZIP "" ON)
option(PHYSFS_ARCHIVE_7Z "" OFF)
option(PHYSFS_ARCHIVE_GRP "" OFF)
option(PHYSFS_ARCHIVE_WAD "" OFF)
option(PHYSFS_ARCHIVE_CSM "" OFF)
option(PHYSFS_ARCHIVE_HOG "" OFF)
option(PHYSFS_ARCHIVE_MVL "" OFF)
option(PHYSFS_ARCHIVE_QPAK "" OFF)
option(PHYSFS_ARCHIVE_SLB "" OFF)
option(PHYSFS_ARCHIVE_VDF "" OFF)
option(PHYSFS_ARCHIVE_ISO9660 "" OFF)
option(PHYSFS_BUILD_STATIC "" ON)
option(PHYSFS_BUILD_SHARED "" OFF)
option(PHYSFS_BUILD_TEST "" OFF)
option(PHYSFS_BUILD_DOCS "" OFF)
if (NOT TARGET PhysicsFS)
    add_subdirectory(physfs physfs)
endif()
target_link_libraries(rana PhysFS::PhysFS-static)

# LuaJIT
find_package(PkgConfig REQUIRED)

pkg_check_modules(LUA REQUIRED luajit)
target_link_libraries(rana ${LUA_LIBRARIES})
target_include_directories(rana PUBLIC ${LUA_INCLUDE_DIRS})
target_compile_options(rana PUBLIC ${LUA_CFLAGS_OTHER})

# imgui
target_include_directories(rana PRIVATE imgui)
target_include_directories(rana PRIVATE imgui/backends)

# others
target_include_directories(rana PRIVATE src)
target_include_directories(rana PRIVATE external)

# compile flags
target_link_libraries(rana -static -static-libgcc -static-libstdc++ -flto -fdiagnostics-color=always)
target_compile_options(rana PRIVATE -ggdb -fdiagnostics-color=always)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_libraries(rana -fansi-escape-codes -fuse-ld=lld -g -Wl,--pdb=)
    target_compile_options(rana PRIVATE -fansi-escape-codes -g -gcodeview)
endif()

if(MSVC)
  target_compile_options(rana PRIVATE /W4)
else()
  target_compile_options(rana PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(RANA_BUILD_CORN "build the 'corn' converter tool" OFF)
if(RANA_BUILD_CORN)
    add_subdirectory(corn)
endif()
