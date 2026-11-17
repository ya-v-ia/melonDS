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

foreach(link_property IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(core_libraries core ${link_property})
    if(core_libraries)
        list(REMOVE_ITEM core_libraries rt "$<LINK_ONLY:rt>")
        set_property(
            TARGET core
            PROPERTY ${link_property} "${core_libraries}"
        )
    endif()
endforeach()
target_include_directories(core BEFORE PRIVATE "$ENV{DEVKITPRO}/libounce/include")

message(STATUS "Experimental Ounce core archive only: no frontend, NRO, or hardware validation")
