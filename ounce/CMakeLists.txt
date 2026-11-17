cmake_minimum_required(VERSION 3.13)

# Invoke through aarch64-none-elf-cmake after sourcing switch2vars.sh.
project(melonDSOunceCore LANGUAGES C CXX)

if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Use the Ounce cross toolchain; see ounce/README.md")
endif()
if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO is not set")
endif()
if(NOT EXISTS "$ENV{DEVKITPRO}/libounce/include/switch.h")
    message(FATAL_ERROR "libounce headers not found in DEVKITPRO/libounce")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This deliberately bypasses the original Switch frontend and root build.
set(ENABLE_JIT OFF CACHE BOOL "JIT requires a separate kaiHEN integration" FORCE)
set(ENABLE_DEKOGPU OFF CACHE BOOL "Do not build deko3d renderers" FORCE)
set(ENABLE_OGLRENDERER OFF CACHE BOOL "Software renderer bring-up only" FORCE)
set(ENABLE_NEONSOFTGPU OFF CACHE BOOL "Disable experimental NEON renderer" FORCE)
set(ARCHITECTURE ARM64)
add_compile_definitions(ARCHITECTURE_ARM64=1 MELONDS_VERSION="0.9.2")

get_filename_component(MELONDS_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_subdirectory("${MELONDS_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}/core")

# The legacy core CMakeLists adds Linux librt when OpenGL is disabled.
# It is not an Ounce dependency. This archive has no executable link step.
get_target_property(CORE_LINK_LIBRARIES core LINK_LIBRARIES)
if(CORE_LINK_LIBRARIES)
    list(REMOVE_ITEM CORE_LINK_LIBRARIES rt)
    set_property(TARGET core PROPERTY LINK_LIBRARIES "${CORE_LINK_LIBRARIES}")
endif()
target_include_directories(core BEFORE PRIVATE "$ENV{DEVKITPRO}/libounce/include")

message(STATUS "Experimental Ounce core archive only: no frontend, NRO, or hardware validation")
