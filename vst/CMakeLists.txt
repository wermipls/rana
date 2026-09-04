cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(ranaplugs C CXX)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    file(GENERATE OUTPUT .gitignore CONTENT "*")
endif()

get_property(multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT CMAKE_BUILD_TYPE AND NOT multiconfig)
    message(STATUS "No build type selected, using Release.")
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
   CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    add_compile_options(-fdiagnostics-color=always)
    add_link_options(-fdiagnostics-color=always)
    if(WIN32)
        add_compile_options(-gcodeview)
        add_link_options(-Wl,--pdb=)
    endif()
endif()

# fixme: let user specify the sdk location.
add_library(vstsdk STATIC EXCLUDE_FROM_ALL
    vstsdk2.4/public.sdk/source/vst2.x/audioeffect.cpp
    vstsdk2.4/public.sdk/source/vst2.x/audioeffectx.cpp
    vstsdk2.4/public.sdk/source/vst2.x/vstplugmain.cpp
)
target_include_directories(vstsdk PUBLIC vstsdk2.4)
target_include_directories(vstsdk PUBLIC vstsdk2.4/public.sdk/source/vst2.x)
target_compile_options(vstsdk PRIVATE -static -Wno-write-strings -Wno-narrowing)

add_library(ranaplugs_common INTERFACE)
target_link_libraries(ranaplugs_common INTERFACE vstsdk)
target_include_directories(ranaplugs_common INTERFACE ../tracy/public)
target_include_directories(ranaplugs_common INTERFACE ../src)
target_compile_options(ranaplugs_common INTERFACE -fno-math-errno -Wall -Wno-format-security)
target_compile_options(ranaplugs_common INTERFACE -static)
target_link_options(ranaplugs_common INTERFACE -static -static-libstdc++ -static-libgcc)
target_compile_definitions(ranaplugs_common INTERFACE PLUGIN_VENDOR="wermi")
target_compile_definitions(ranaplugs_common INTERFACE DOCTEST_CONFIG_DISABLE)

file(GLOB sources *.cpp)
foreach(source ${sources})
    # ew.
    file(RELATIVE_PATH source_relative ${CMAKE_SOURCE_DIR} ${source})
    string(REGEX REPLACE "\.cpp" "" source_noext ${source_relative})
    string(CONCAT target_name "rana" ${source_noext})

    add_library(${target_name} MODULE
        ${source}
    )
    target_link_libraries(${target_name} PRIVATE ranaplugs_common)
    set_target_properties(${target_name} PROPERTIES PREFIX "")
endforeach()
