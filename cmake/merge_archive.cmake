# merge_archive.cmake - generic .a merge tool
# Usage: cmake -DALXLIB_ARCHIVE=output.a -DTHIRD_PARTY_LIBS_FILE=list.txt -DAR=/usr/bin/ar -P merge_archive.cmake

cmake_minimum_required(VERSION 3.16)

# Read the list of .a files to merge from a file
if(THIRD_PARTY_LIBS_FILE)
    file(READ ${THIRD_PARTY_LIBS_FILE} CONTENT)
    string(REGEX REPLACE "\n" ";" ARCHIVE_LIST "${CONTENT}")
endif()

# Temporary directory (next to the output file)
get_filename_component(OUTPUT_DIR ${ALXLIB_ARCHIVE} DIRECTORY)
get_filename_component(OUTPUT_NAME ${ALXLIB_ARCHIVE} NAME_WE)
set(MERGE_DIR ${OUTPUT_DIR}/merge_${OUTPUT_NAME}_tmp)
execute_process(COMMAND rm -rf ${MERGE_DIR})
file(MAKE_DIRECTORY ${MERGE_DIR})

# Extract the .o files from every .a
foreach(ARCHIVE ${ARCHIVE_LIST})
    if(EXISTS ${ARCHIVE})
        execute_process(
            COMMAND ${AR} x ${ARCHIVE}
            WORKING_DIRECTORY ${MERGE_DIR}
            RESULT_VARIABLE RET
        )
    endif()
endforeach()

# Append to the target .a (existing content is kept)
file(GLOB ALL_OBJECTS "${MERGE_DIR}/*.o")
if(ALL_OBJECTS)
    execute_process(
        COMMAND ${AR} rcs ${ALXLIB_ARCHIVE} ${ALL_OBJECTS}
        WORKING_DIRECTORY ${MERGE_DIR}
        RESULT_VARIABLE RET
    )
endif()

# Clean up the temporary directory
execute_process(COMMAND rm -rf ${MERGE_DIR})
