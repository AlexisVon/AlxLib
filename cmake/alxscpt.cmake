# alxscpt - script engine

file(GLOB ALXSCPT_SOURCES
    ${CMAKE_SOURCE_DIR}/source/alxscpt/*.cpp
    ${CMAKE_SOURCE_DIR}/source/alxscpt/script/*.cpp
)

set(ALXSCPT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alxbase
    ${CMAKE_SOURCE_DIR}/include/alxcore
    ${CMAKE_SOURCE_DIR}/include/alxscpt
)

# OBJECT library
if(ALXSCPT_ENABLE OR ALXSCPT_STATIC OR ALXLIB_STATIC)
    add_library(alxscpt_obj OBJECT ${ALXSCPT_SOURCES})
    target_include_directories(alxscpt_obj PUBLIC ${ALXSCPT_INCLUDE_DIRS})
    target_include_directories(alxscpt_obj PRIVATE ${CMAKE_SOURCE_DIR}/source/alxscpt)
    target_compile_features(alxscpt_obj PUBLIC cxx_std_17)
    set_target_properties(alxscpt_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# Shared library
if(ALXSCPT_ENABLE)
    add_library(alxscpt SHARED $<TARGET_OBJECTS:alxscpt_obj>)
    target_include_directories(alxscpt PUBLIC ${ALXSCPT_INCLUDE_DIRS})
    target_compile_features(alxscpt PUBLIC cxx_std_17)
    target_compile_definitions(alxscpt PRIVATE ALXSCPTLIB_EXPORTS)
    target_link_libraries(alxscpt PUBLIC alxbase alxcore)
    set_target_properties(alxscpt PROPERTIES
        OUTPUT_NAME alxscpt
        VERSION ${ALXLIB_VERSION}
        SOVERSION ${ALXLIB_SOVERSION}
    )
endif()

# Static library (scpt + core + base + 3rd party)
# 3rd party is the same as core's
if(ALXSCPT_STATIC)
    # Write 3rd party list (same as core)
    string(REPLACE ";" "\n" ALXSCPT_3RD_PARTY_CONTENT "${ALXCORE_3RD_PARTY}")
    file(WRITE ${CMAKE_BINARY_DIR}/alxscpt_3rdparty.txt "${ALXSCPT_3RD_PARTY_CONTENT}")

    add_library(alxscpt_static STATIC
        $<TARGET_OBJECTS:alxbase_obj>
        $<TARGET_OBJECTS:alxcore_obj>
        $<TARGET_OBJECTS:alxscpt_obj>
    )
    target_include_directories(alxscpt_static PUBLIC ${ALXSCPT_INCLUDE_DIRS})
    target_compile_features(alxscpt_static PUBLIC cxx_std_17)
    target_compile_definitions(alxscpt_static PRIVATE ALEXISLIB_STATIC=1)
    set_target_properties(alxscpt_static PROPERTIES OUTPUT_NAME alxscpt)

    # Merge 3rd party
    add_custom_command(TARGET alxscpt_static POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DALXLIB_ARCHIVE=$<TARGET_FILE:alxscpt_static>
            -DTHIRD_PARTY_LIBS_FILE=${CMAKE_BINARY_DIR}/alxscpt_3rdparty.txt
            -DAR=${CMAKE_AR}
            -P ${CMAKE_SOURCE_DIR}/cmake/merge_archive.cmake
        COMMENT "Merging 3rd party libraries into alxscpt.a"
    )
endif()
