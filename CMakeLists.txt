cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(rana C CXX)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    # Git auto-ignore out-of-source build directory
    file(GENERATE OUTPUT .gitignore CONTENT "*")
endif()

set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)

include(ExternalProject)

add_executable(rana WIN32
    src/main.cpp
    src/fs.cpp
    src/font.cpp
    src/gfx.cpp
    src/gfx_textures.cpp
    src/shader.cpp
    src/input.cpp
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
    tracy/public/TracyClient.cpp
)

# compile flags
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "-s")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-s")

target_link_libraries(rana -static-libgcc -static-libstdc++ -fdiagnostics-color=always)
target_compile_options(rana PRIVATE -ggdb -fdiagnostics-color=always)
if(WIN32)
    target_link_libraries(rana -static)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -fansi-escape-codes -fuse-ld=lld -g -Wl,/debug,--pdb=,--gc-sections")
    string(APPEND CMAKE_C_FLAGS " -fansi-escape-codes -g -gcodeview")
    string(APPEND CMAKE_CXX_FLAGS " -fansi-escape-codes -g -gcodeview")
    # gcc seems to have some trouble with lto...
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -flto=thin")
    string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " -flto=thin")
    string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL " -flto=thin")
    string(APPEND CMAKE_C_FLAGS_RELEASE " -flto=thin")
    string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO " -flto=thin")
    string(APPEND CMAKE_C_FLAGS_MINSIZEREL " -flto=thin")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " -flto=thin -Wl,-O2,--lto-O3,--lto-CGO3,--icf=all,--strip-all")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO " -flto=thin -Wl,-O2,--lto-O3,--lto-CGO3,--icf=all")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_MINSIZEREL " -flto=thin -Wl,-O2,--lto-O3,--lto-CGO3,--icf=all,--strip-all")
endif()

if(MSVC)
  target_compile_options(rana PRIVATE /W4)
else()
  target_compile_options(rana PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(rana PRIVATE -fmacro-prefix-map=${PROJECT_SOURCE_DIR}=rana)
  string(APPEND CMAKE_CXX_FLAGS " -ffunction-sections -fdata-sections")
  string(APPEND CMAKE_C_FLAGS " -ffunction-sections -fdata-sections")
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--gc-sections")
endif()

# handle dependencies
# SDL3
option(SDL_SHARED "" OFF)
option(SDL_STATIC "" ON)
# disable a bunch of stuff we don't use to reduce binary size
option(SDL_CAMERA "" OFF)
option(SDL_DIRECTX "" OFF)
option(SDL_DISKAUDIO "" OFF)
option(SDL_DUMMYAUDIO "" OFF)
option(SDL_DUMMYCAMERA "" OFF)
option(SDL_DUMMYVIDEO "" OFF)
option(SDL_GPU "" OFF)
option(SDL_HAPTIC "" OFF)
option(SDL_HIDAPI "" OFF)
option(SDL_HIDAPI_JOYSTICK "" OFF)
option(SDL_HIDAPI_LIBUSB "" OFF)
option(SDL_HIDAPI_LIBUSB_SHARED "" OFF)
option(SDL_OFFSCREEN "" OFF)
option(SDL_OPENGLES "" OFF)
option(SDL_POWER "" OFF)
option(SDL_RENDER "" OFF)
option(SDL_RENDER_D3D "" OFF)
option(SDL_RENDER_METAL "" OFF)
option(SDL_RENDER_VULKAN "" OFF)
option(SDL_SENSOR "" OFF)
option(SDL_TEST_LIBRARY "" OFF)
option(SDL_VULKAN "" OFF)
add_subdirectory(SDL EXCLUDE_FROM_ALL)
target_link_libraries(rana SDL3::SDL3)
# for some reason, SDL_LEAN_AND_MEAN cmake switch is only exposed for
# the ngage build, so we have to set it like this
sdl_compile_definitions(PRIVATE SDL_LEAN_AND_MEAN)

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
set(PHYSFS_TARGETNAME_UNINSTALL "physfs_uninstall" CACHE STRING "")
if (NOT TARGET PhysicsFS)
    add_subdirectory(physfs physfs EXCLUDE_FROM_ALL)
endif()
target_link_libraries(rana PhysFS::PhysFS-static)

# imgui
target_include_directories(rana PRIVATE imgui)
target_include_directories(rana PRIVATE imgui/backends)

# tracy
target_include_directories(rana PRIVATE tracy/public)
if(MSYS OR MINGW)
    target_link_libraries(rana ws2_32 dbghelp)
endif()

# glm
add_subdirectory(glm glm)
target_link_libraries(rana glm::glm)

# LuaJIT
# this works... with gcc only
if(CMAKE_CXX_COMPILER_ID STREQUAL "GCC")
    # Copy LuaJIT source to binary dir. LuaJIT builds in-source,
    # and we don't want to modify the original source tree, so
    # we copy it.
    file(COPY LuaJIT DESTINATION ${CMAKE_BINARY_DIR}/libs)

    ExternalProject_Add(luajit
        SOURCE_DIR "${CMAKE_BINARY_DIR}/libs/LuaJIT"
        BUILD_IN_SOURCE true
        CONFIGURE_COMMAND ""
        BUILD_COMMAND "make"
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/libs/LuaJIT/src/libluajit-5.1.dll.a"
    )
    add_library(LuaJIT INTERFACE)
    add_dependencies(LuaJIT luajit)
    target_include_directories(LuaJIT INTERFACE LuaJIT/src)
    target_link_libraries(LuaJIT INTERFACE "${CMAKE_BINARY_DIR}/libs/LuaJIT/src/libluajit-5.1.dll.a")

    target_include_directories(rana PRIVATE LuaJIT)
    target_link_libraries(rana LuaJIT)
else()
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(LUA REQUIRED luajit)
    target_link_libraries(rana ${LUA_LIBRARIES})
    target_include_directories(rana PUBLIC ${LUA_INCLUDE_DIRS})
    target_compile_options(rana PUBLIC ${LUA_CFLAGS_OTHER})
endif()

# sol
target_include_directories(rana PRIVATE sol2/include)

# others
target_include_directories(rana PRIVATE src)
target_include_directories(rana PRIVATE external)

# configuration toggles
option(RANA_BUILD_CORN "build the 'corn' converter tool" OFF)
if(RANA_BUILD_CORN)
    add_subdirectory(corn)
endif()

option(RANA_TRACY "build with Tracy profiler support" OFF)
if(RANA_TRACY)
    target_compile_options(rana PRIVATE -DTRACY_ENABLE)
endif()
