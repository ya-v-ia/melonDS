cmake_minimum_required(VERSION 3.13)
project(melonDSOunceRunner LANGUAGES C CXX)
if(NOT CMAKE_CROSSCOMPILING OR NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "Source switch2vars.sh and use aarch64-none-elf-cmake")
endif()

add_subdirectory(.. ounce-core)

find_library(OUNCE_RUNTIME NAMES ounce nx PATHS "$ENV{DEVKITPRO}/libounce/lib" NO_DEFAULT_PATH)
find_library(OUNCE_EGL NAMES EGL PATHS "$ENV{DEVKITPRO}/portlibs/switch2/lib" "$ENV{DEVKITPRO}/libounce/lib" NO_DEFAULT_PATH)
find_program(ELF2NRO elf2nro HINTS "$ENV{DEVKITPRO}/tools/bin")
if(NOT OUNCE_RUNTIME OR NOT OUNCE_EGL OR NOT ELF2NRO)
    message(FATAL_ERROR "Missing libounce runtime, Ounce EGL library, or elf2nro; check installed SDK packages")
endif()

add_executable(melonds-ounce-runner main.cpp Platform.cpp)
set_target_properties(melonds-ounce-runner PROPERTIES SUFFIX ".elf" CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(melonds-ounce-runner BEFORE PRIVATE
    "$ENV{DEVKITPRO}/portlibs/switch2/include"
    "$ENV{DEVKITPRO}/libounce/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../src"
)
target_compile_options(melonds-ounce-runner PRIVATE -Wall -Wextra)
target_link_libraries(melonds-ounce-runner PRIVATE core ${OUNCE_EGL} ${OUNCE_RUNTIME} m)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/melonds-ounce-runner.nro"
    COMMAND "${ELF2NRO}" $<TARGET_FILE:melonds-ounce-runner> "${CMAKE_CURRENT_BINARY_DIR}/melonds-ounce-runner.nro"
    DEPENDS melonds-ounce-runner VERBATIM)
add_custom_target(ounce-runner-nro ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/melonds-ounce-runner.nro")
