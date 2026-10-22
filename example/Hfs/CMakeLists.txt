cmake_minimum_required(VERSION 3.16)
project(Hfs VERSION 1.0.0 LANGUAGES CXX)

option(HFS_ENABLE_DELETE "Enable the delete API and its page buttons" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ALXLIB_INC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
set(ALXLIB_INC_BASE ${ALXLIB_INC}/alxbase)
set(ALXLIB_INC_CORE ${ALXLIB_INC}/alxcore)
set(ALXLIB_INC_COMM ${ALXLIB_INC}/alxcomm)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/res.cpp
    src/filesys.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    HFS_ENABLE_DELETE=$<BOOL:${HFS_ENABLE_DELETE}>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${ALXLIB_INC}
    ${ALXLIB_INC_BASE}
    ${ALXLIB_INC_CORE}
    ${ALXLIB_INC_COMM}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    alxcomm
    alxcore
    alxbase
)

# ALXLIB_BIN_PATH comes from the top-level build; fall back to a local bin/
if(NOT ALXLIB_BIN_PATH)
    set(ALXLIB_BIN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${ALXLIB_BIN_PATH}
)

# Always run: page assets are not compilation inputs, and a POST_BUILD copy
# would only refresh them when the executable itself relinks.
add_custom_target(${PROJECT_NAME}_page ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/page
        ${ALXLIB_BIN_PATH}/Hfs.page
    COMMENT "Copying Hfs page resources"
)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_page)
