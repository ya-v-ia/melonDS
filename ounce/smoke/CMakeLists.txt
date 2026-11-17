cmake_minimum_required(VERSION 3.13)
project(melonDSOunceSmoke LANGUAGES C CXX)
add_subdirectory(.. ounce-core)

find_library(OUNCE_RUNTIME NAMES ounce_core nx PATHS "$ENV{DEVKITPRO}/libounce/lib" NO_DEFAULT_PATH)
if(NOT OUNCE_RUNTIME)
    message(FATAL_ERROR "No Ounce runtime library found")
endif()
find_program(ELF2NRO elf2nro HINTS "$ENV{DEVKITPRO}/tools/bin")
if(NOT ELF2NRO)
    message(FATAL_ERROR "elf2nro not found")
endif()

add_executable(melonds-ounce-smoke main.cpp Platform.cpp)
set_target_properties(melonds-ounce-smoke PROPERTIES SUFFIX ".elf" CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(melonds-ounce-smoke BEFORE PRIVATE "$ENV{DEVKITPRO}/libounce/include" "${CMAKE_CURRENT_LIST_DIR}/../../src")
target_compile_options(melonds-ounce-smoke PRIVATE -Wall -Wextra)
target_link_libraries(melonds-ounce-smoke PRIVATE core ${OUNCE_RUNTIME} m)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/melonds-ounce-smoke.nro"
    COMMAND "${ELF2NRO}" $<TARGET_FILE:melonds-ounce-smoke> "${CMAKE_CURRENT_BINARY_DIR}/melonds-ounce-smoke.nro"
    DEPENDS melonds-ounce-smoke VERBATIM)
add_custom_target(ounce-smoke-nro ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/melonds-ounce-smoke.nro")
