# alxlib - the complete static library (merges alxcomm.a + alxscpt.a)

if(NOT TARGET alxcomm_static OR NOT TARGET alxscpt_static)
    message(FATAL_ERROR "alxlib requires ALXCOMM_STATIC and ALXSCPT_STATIC to be ON")
endif()

set(ALXLIB_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alxbase
    ${CMAKE_SOURCE_DIR}/include/alxcore
    ${CMAKE_SOURCE_DIR}/include/alxscpt
    ${CMAKE_SOURCE_DIR}/include/alxcomm
)

# Write list file (same as other modules)
file(WRITE ${CMAKE_BINARY_DIR}/alxlib_list.txt
    "${ALXLIB_BIN_PATH}/libalxcomm.a\n${ALXLIB_BIN_PATH}/libalxscpt.a")

# Custom target to merge comm.a + scpt.a
add_custom_target(alxlib ALL
    COMMAND ${CMAKE_COMMAND}
        -DALXLIB_ARCHIVE=${ALXLIB_BIN_PATH}/libalxlib.a
        -DTHIRD_PARTY_LIBS_FILE=${CMAKE_BINARY_DIR}/alxlib_list.txt
        -DAR=${CMAKE_AR}
        -P ${CMAKE_SOURCE_DIR}/cmake/merge_archive.cmake
    DEPENDS alxcomm_static alxscpt_static
    COMMENT "Creating monolithic alxlib.a"
)
