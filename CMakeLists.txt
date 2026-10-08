cmake_minimum_required(VERSION 3.16)
project(AlxLib VERSION 1.0.1 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Compiler / standard
# ---------------------------------------------------------------------------
if(NOT CMAKE_CXX_COMPILER)
    find_program(GXX g++)
    if(GXX)
        set(CMAKE_CXX_COMPILER ${GXX})
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for IDE/tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type - use empty to avoid adding default flags
set(CMAKE_BUILD_TYPE "" CACHE STRING "" FORCE)

# Optimization level
add_compile_options(-O${ALXLIB_OPT_LEVEL})
if(ALXLIB_SPLIT_DEBUG)
    add_compile_options(-g)
endif()

# Output directories (ALXLIB_BIN_DIR relative to the source dir, or absolute)
set(ALXLIB_BIN_DIR "bin" CACHE STRING "Build output directory")
if(NOT ALXLIB_BIN_DIR)
    message(FATAL_ERROR "ALXLIB_BIN_DIR must not be empty")
endif()
# CMake does not expand a leading ~, the shell only does it when unquoted
string(REGEX REPLACE "^~" "$ENV{HOME}" ALXLIB_BIN_DIR "${ALXLIB_BIN_DIR}")
if(IS_ABSOLUTE "${ALXLIB_BIN_DIR}")
    set(ALXLIB_BIN_PATH "${ALXLIB_BIN_DIR}")
else()
    set(ALXLIB_BIN_PATH "${CMAKE_SOURCE_DIR}/${ALXLIB_BIN_DIR}")
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ALXLIB_BIN_PATH})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ALXLIB_BIN_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ALXLIB_BIN_PATH})

# ---------------------------------------------------------------------------
# Version (bump it here)
# ---------------------------------------------------------------------------
# One version for the whole library: every module carries it in its filename, and the
# SONAME is major.minor -- so a release that changed any module refuses to load beside
# an older one instead of failing somewhere at run time.
set(ALXLIB_VERSION_MAJOR 1 CACHE STRING "Library major version")
set(ALXLIB_VERSION_MINOR 1 CACHE STRING "Library minor version")
set(ALXLIB_VERSION_PATCH 0 CACHE STRING "Library patch version")

set(ALXLIB_VERSION "${ALXLIB_VERSION_MAJOR}.${ALXLIB_VERSION_MINOR}.${ALXLIB_VERSION_PATCH}")
set(ALXLIB_SOVERSION "${ALXLIB_VERSION_MAJOR}.${ALXLIB_VERSION_MINOR}")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ALXCOMM_TLS "Enable TLS support for alxcomm" ON)
option(ALXLIB_SPLIT_DEBUG "Separate debug info into .debug files" OFF)
set(ALXLIB_OPT_LEVEL "3" CACHE STRING "Optimization level (0/1/2/3)")
set_property(CACHE ALXLIB_OPT_LEVEL PROPERTY STRINGS 0 1 2 3)

option(ALXBASE_ENABLE "Build alxbase" ON)
option(ALXCORE_ENABLE "Build alxcore" ON)
option(ALXSCPT_ENABLE "Build alxscpt" ON)
option(ALXCOMM_ENABLE "Build alxcomm" ON)

option(ALXBASE_STATIC "Build alxbase.a" OFF)
option(ALXCORE_STATIC "Build alxcore.a" OFF)
option(ALXSCPT_STATIC "Build alxscpt.a" OFF)
option(ALXCOMM_STATIC "Build alxcomm.a" OFF)

option(ALXLIB_STATIC "Build alxlib.a (all-in-one)" OFF)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_EXAMPLE_SCPT "Build Scpt example" ON)
option(BUILD_EXAMPLE_LZA "Build Lza example" ON)

# ---------------------------------------------------------------------------
# Dependency resolution
# ---------------------------------------------------------------------------
# alxlib requires comm + scpt
if(ALXLIB_STATIC)
    set(ALXCOMM_STATIC ON)
    set(ALXSCPT_STATIC ON)
    set(ALXCORE_ENABLE ON)
    set(ALXBASE_ENABLE ON)
endif()

# Example dependencies
if(BUILD_EXAMPLES)
    if(BUILD_EXAMPLE_SCPT)
        set(ALXSCPT_ENABLE ON)
    endif()
    if(BUILD_EXAMPLE_LZA)
        set(ALXBASE_ENABLE ON)
        set(ALXCORE_ENABLE ON)
    endif()
endif()

# Module dependencies
if(ALXCOMM_ENABLE OR ALXCOMM_STATIC)
    set(ALXCORE_ENABLE ON)
    set(ALXBASE_ENABLE ON)
endif()
if(ALXSCPT_ENABLE OR ALXSCPT_STATIC)
    set(ALXCORE_ENABLE ON)
    set(ALXBASE_ENABLE ON)
endif()
if(ALXCORE_ENABLE OR ALXCORE_STATIC)
    set(ALXBASE_ENABLE ON)
endif()

# ---------------------------------------------------------------------------
# Libraries
# ---------------------------------------------------------------------------
include(cmake/alxbase.cmake)
include(cmake/alxcore.cmake)
include(cmake/alxscpt.cmake)
include(cmake/alxcomm.cmake)

if(ALXLIB_STATIC)
    include(cmake/alxlib.cmake)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTS AND CMAKE_CXX_STANDARD VERSION_LESS "17")
    message(FATAL_ERROR "BUILD_TESTS requires C++17 or later (current: C++${CMAKE_CXX_STANDARD})")
endif()

if(BUILD_TESTS)
    enable_testing()
    if(ALXBASE_ENABLE)
        add_subdirectory(${CMAKE_SOURCE_DIR}/gtest/alxbase ${CMAKE_BINARY_DIR}/gtest/alxbase)
    endif()
    if(ALXCORE_ENABLE)
        add_subdirectory(${CMAKE_SOURCE_DIR}/gtest/alxcore ${CMAKE_BINARY_DIR}/gtest/alxcore)
    endif()
    if(ALXSCPT_ENABLE)
        add_subdirectory(${CMAKE_SOURCE_DIR}/gtest/alxscpt ${CMAKE_BINARY_DIR}/gtest/alxscpt)
    endif()
    if(ALXCOMM_ENABLE)
        add_subdirectory(${CMAKE_SOURCE_DIR}/gtest/alxcomm ${CMAKE_BINARY_DIR}/gtest/alxcomm)
    endif()
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(BUILD_EXAMPLES)
    if(BUILD_EXAMPLE_SCPT)
        add_subdirectory(${CMAKE_SOURCE_DIR}/example/Scpt ${CMAKE_BINARY_DIR}/example/Scpt)
    endif()
    if(BUILD_EXAMPLE_LZA)
        add_subdirectory(${CMAKE_SOURCE_DIR}/example/Lza ${CMAKE_BINARY_DIR}/example/Lza)
    endif()
endif()

# ---------------------------------------------------------------------------
# Split debug info (optional)
# ---------------------------------------------------------------------------
if(ALXLIB_SPLIT_DEBUG)
    # Add post-build command to all targets
    foreach(target alxbase alxcore alxscpt alxcomm)
        if(TARGET ${target})
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND objcopy --only-keep-debug $<TARGET_FILE:${target}> $<TARGET_FILE:${target}>.debug
                COMMAND objcopy --strip-debug $<TARGET_FILE:${target}>
                COMMAND objcopy --add-gnu-debuglink=$<TARGET_FILE:${target}>.debug $<TARGET_FILE:${target}>
                COMMENT "Splitting debug info for ${target}"
            )
        endif()
    endforeach()
endif()
