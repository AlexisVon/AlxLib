cmake_minimum_required(VERSION 3.16)
project(Scpt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ALXLIB_INC ${CMAKE_SOURCE_DIR}/../../include)
set(ALXLIB_INC_BASE ${CMAKE_SOURCE_DIR}/../../include/alxbase)
set(ALXLIB_INC_CORE ${CMAKE_SOURCE_DIR}/../../include/alxcore)
set(ALXLIB_INC_SCPT ${CMAKE_SOURCE_DIR}/../../include/alxscpt)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/line_editor.cpp
    src/line_editor.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${ALXLIB_INC}
    ${ALXLIB_INC_BASE}
    ${ALXLIB_INC_CORE}
    ${ALXLIB_INC_SCPT}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    alxscpt
    alxcore
    alxbase
)

# ALXLIB_BIN_PATH comes from the top-level build; fall back to a local bin/
if(NOT ALXLIB_BIN_PATH)
    set(ALXLIB_BIN_PATH ${CMAKE_SOURCE_DIR}/bin)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${ALXLIB_BIN_PATH}
)

# Link modules for the .axc corpus. These are dlopen'd by the engine, so they are
# deliberately NOT linked against alxscpt — the host supplies those symbols at load time.
# Linux only: the corpus spells `link "base.so"`, and the MSVC build does not run it.
add_library(base MODULE
    link/base/base.cpp
    link/base/extra.cpp
    link/base/filesys.cpp
    link/base/math.cpp
    link/base/string.cpp
    link/base/system.cpp
)
add_library(test_store MODULE test/link/test_store.cpp)

# CMAKE_CURRENT_SOURCE_DIR, not CMAKE_SOURCE_DIR: the latter is the repo root when this
# directory is built in-tree, and the ALXLIB_INC_* above are spelled for a standalone build.
# The Scpt target above does not notice because linking alxscpt brings these in transitively.
set(_mod_incs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/alxbase
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/alxcore
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/alxscpt
)
foreach(_mod base test_store)
    target_include_directories(${_mod} PRIVATE ${_mod_incs})
    set_target_properties(${_mod} PROPERTIES
        PREFIX ""   # the corpus loads "base.so", not "libbase.so"
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test/bin
    )
endforeach()
