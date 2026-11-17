cmake_minimum_required(VERSION 3.13)
project(melonDSOunceDisplay LANGUAGES C CXX)
if(NOT CMAKE_CROSSCOMPILING OR NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "Source switch2vars.sh and use aarch64-none-elf-cmake")
endif()
find_library(
    OUNCE_RUNTIME
    NAMES ounce nx
    PATHS "$ENV{DEVKITPRO}/libounce/lib"
    NO_DEFAULT_PATH
)
find_library(OUNCE_EGL NAMES EGL PATHS "$ENV{DEVKITPRO}/portlibs/switch2/lib" "$ENV{DEVKITPRO}/libounce/lib" NO_DEFAULT_PATH)
find_program(ELF2NRO elf2nro HINTS "$ENV{DEVKITPRO}/tools/bin")
if(NOT OUNCE_RUNTIME OR NOT OUNCE_EGL OR NOT ELF2NRO)
    message(FATAL_ERROR "Missing libounce runtime, Ounce EGL library, or elf2nro; check installed SDK packages")
endif()
add_executable(melonds-ounce-display main.cpp)
set_target_properties(melonds-ounce-display PROPERTIES SUFFIX ".elf" CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(melonds-ounce-display BEFORE PRIVATE "$ENV{DEVKITPRO}/portlibs/switch2/include" "$ENV{DEVKITPRO}/libounce/include")
target_compile_options(melonds-ounce-display PRIVATE -Wall -Wextra)
target_link_libraries(melonds-ounce-display PRIVATE ${OUNCE_EGL} ${OUNCE_RUNTIME} m)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/melonds-ounce-display.nro"
    COMMAND "${ELF2NRO}" $<TARGET_FILE:melonds-ounce-display> "${CMAKE_CURRENT_BINARY_DIR}/melonds-ounce-display.nro"
    DEPENDS melonds-ounce-display VERBATIM)
add_custom_target(ounce-display-nro ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/melonds-ounce-display.nro")
