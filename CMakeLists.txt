cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(rana C CXX)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    # Git auto-ignore out-of-source build directory
    file(GENERATE OUTPUT .gitignore CONTENT "*")
endif()

get_property(multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT CMAKE_BUILD_TYPE AND NOT multiconfig)
    message(STATUS "No build type selected, using MinSizeRel.")
    set(CMAKE_BUILD_TYPE "MinSizeRel")
endif()

set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# no exceptions or rtti for main/library code.
add_compile_options(-fno-exceptions -fno-rtti)

add_library(rana_common INTERFACE)

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
    external/glad/glad.c
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/backends/imgui_impl_sdl3.cpp
    tracy/public/TracyClient.cpp
)

add_executable(rana_test
    src/rana_test.cpp
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

# doctest technically can work without exceptions, but in a limited way,
# so we just turn exceptions back on for the test binary.
target_compile_options(rana_test PRIVATE -fexceptions)

target_link_libraries(rana_common INTERFACE -static-libgcc -static-libstdc++ -fdiagnostics-color=always)
target_compile_options(rana_common INTERFACE -ggdb -fdiagnostics-color=always)
if(WIN32)
    target_link_libraries(rana_common INTERFACE -static)
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
  target_compile_options(rana_common INTERFACE /W4)
else()
  target_compile_options(rana_common INTERFACE -Wall -Wextra -Wpedantic)
  target_compile_options(rana_common INTERFACE -fmacro-prefix-map=${PROJECT_SOURCE_DIR}=rana)
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
target_link_libraries(rana_common INTERFACE SDL3::SDL3)
# for some reason, SDL_LEAN_AND_MEAN cmake switch is only exposed for
# the ngage build, so we have to set it like this
sdl_compile_definitions(PRIVATE SDL_LEAN_AND_MEAN)

# libopus
if (NOT TARGET opus)
    add_subdirectory(opus opus)
endif()
target_link_libraries(rana_common INTERFACE opus)

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
target_link_libraries(rana_common INTERFACE PhysFS::PhysFS-static)

# libjxl
option(RANA_ENABLE_JXL "enable JPEG XL image decoder" ON)
if(RANA_ENABLE_JXL)
    option(BUILD_TESTING "" OFF) # sick namespace pollution
    option(JPEGXL_ENABLE_FUZZERS "" OFF)
    option(JPEGXL_ENABLE_TOOLS "" OFF)
    option(JPEGXL_ENABLE_JPEGLI "" OFF)
    option(JPEGXL_ENABLE_JPEGLI_LIBJPEG "" OFF)
    option(JPEGXL_ENABLE_DOXYGEN "" OFF)
    option(JPEGXL_ENABLE_MANPAGES "" OFF)
    option(JPEGXL_ENABLE_BENCHMARK "" OFF)
    option(JPEGXL_ENABLE_EXAMPLES "" OFF)
    option(JPEGXL_ENABLE_LIBPNG "" OFF)
    option(JPEGXL_ENABLE_JNI "" OFF)
    option(JPEGXL_ENABLE_SJPEG "" OFF)
    option(JPEGXL_ENABLE_OPENEXR "" OFF)
    option(JPEGXL_ENABLE_TRANSCODE_JPEG "" OFF)
    option(JPEGXL_ENABLE_BOXES "" OFF)
    option(JPEGXL_STATIC "" ON)
    # for now we keep only SSE2 enabled for size reasons.
    # that might change to SSE4, since most CPUs from last ~15 years support it.
    option(JPEGXL_ENABLE_HWY_AVX2 "" OFF)
    option(JPEGXL_ENABLE_HWY_AVX3_DL "" OFF)
    option(JPEGXL_ENABLE_HWY_SSE4 "" OFF)

    add_subdirectory(libjxl libjxl EXCLUDE_FROM_ALL)
    target_link_libraries(rana_common INTERFACE jxl_dec)
    target_compile_definitions(rana_common INTERFACE RANA_ENABLE_JXL)
endif()

# imgui
target_include_directories(rana_common INTERFACE imgui)
target_include_directories(rana_common INTERFACE imgui/backends)

# tracy
target_include_directories(rana_common INTERFACE tracy/public)
if(MSYS OR MINGW)
    target_link_libraries(rana_common INTERFACE ws2_32 dbghelp)
endif()

# glm
add_subdirectory(glm glm)
target_link_libraries(rana_common INTERFACE glm::glm)

# LuaJIT
option(
    LUAJIT_ALWAYS_BUILD
    "Run the LuaJIT make unconditionally. Useful when making changes to the library. Disabled by default."
    OFF
)
ExternalProject_Add(luajit
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/LuaJIT/src"
    BUILD_IN_SOURCE true
    CONFIGURE_COMMAND make clean
    BUILD_COMMAND make amalg BUILDMODE=static CC=${CMAKE_C_COMPILER}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "${CMAKE_SOURCE_DIR}/LuaJIT/src/libluajit.a"
    BUILD_ALWAYS ${LUAJIT_ALWAYS_BUILD}
)
add_library(LuaJIT INTERFACE)
add_dependencies(LuaJIT luajit)
target_include_directories(LuaJIT INTERFACE "${CMAKE_SOURCE_DIR}/LuaJIT/src")
target_link_libraries(LuaJIT INTERFACE "${CMAKE_SOURCE_DIR}/LuaJIT/src/libluajit.a")

target_include_directories(rana_common INTERFACE "${CMAKE_SOURCE_DIR}/LuaJIT")
target_link_libraries(rana_common INTERFACE LuaJIT)

# sol
target_include_directories(rana_common INTERFACE sol2/include)

# doctest
add_subdirectory(doctest)
target_link_libraries(rana_common INTERFACE doctest)
target_compile_definitions(rana PUBLIC DOCTEST_CONFIG_DISABLE) # no tests in main build.

# others
target_include_directories(rana_common INTERFACE src)
target_include_directories(rana_common INTERFACE external)

# configuration toggles
option(RANA_BUILD_CORN "build the 'corn' converter tool" OFF)
if(RANA_BUILD_CORN)
    add_subdirectory(corn)
endif()

option(RANA_TRACY "build with Tracy profiler support" OFF)
if(RANA_TRACY)
    target_compile_options(rana PRIVATE -DTRACY_ENABLE)
endif()

# use common libs/flags.
target_link_libraries(rana PRIVATE rana_common)
target_link_libraries(rana_test PRIVATE rana_common)
