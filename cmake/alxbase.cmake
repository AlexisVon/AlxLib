# alxbase - basic types + serialization

set(ALXBASE_SOURCES
    ${CMAKE_SOURCE_DIR}/source/alxbase/aalgo.cpp
    ${CMAKE_SOURCE_DIR}/source/alxbase/aaes.cpp
    ${CMAKE_SOURCE_DIR}/source/alxbase/abytes.cpp
    ${CMAKE_SOURCE_DIR}/source/alxbase/acsv.cpp
    ${CMAKE_SOURCE_DIR}/source/alxbase/ajson.cpp
    ${CMAKE_SOURCE_DIR}/source/alxbase/aregex_ex.cpp
    ${CMAKE_SOURCE_DIR}/source/alxbase/astring.cpp
    ${CMAKE_SOURCE_DIR}/source/alxbase/avarsolid.cpp
    ${CMAKE_SOURCE_DIR}/source/alxbase/averify.cpp
    ${CMAKE_SOURCE_DIR}/source/alxbase/axml.cpp
)

set(ALXBASE_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alxbase
)

# OBJECT library
if(ALXBASE_ENABLE OR ALXBASE_STATIC OR ALXLIB_STATIC)
    add_library(alxbase_obj OBJECT ${ALXBASE_SOURCES})
    target_include_directories(alxbase_obj PUBLIC ${ALXBASE_INCLUDE_DIRS})
    target_compile_features(alxbase_obj PUBLIC cxx_std_17)
    set_target_properties(alxbase_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # SIMD support for crypto
    set_source_files_properties(${CMAKE_SOURCE_DIR}/source/alxbase/aaes.cpp PROPERTIES COMPILE_FLAGS "-maes -mpclmul -msse4.1")
    set_source_files_properties(${CMAKE_SOURCE_DIR}/source/alxbase/averify.cpp PROPERTIES COMPILE_FLAGS "-msha -msse4.1 -mcrc32")
endif()

# Shared library
if(ALXBASE_ENABLE)
    add_library(alxbase SHARED $<TARGET_OBJECTS:alxbase_obj>)
    target_include_directories(alxbase PUBLIC ${ALXBASE_INCLUDE_DIRS})
    target_compile_features(alxbase PUBLIC cxx_std_17)
    target_compile_definitions(alxbase PRIVATE ALXBASELIB_EXPORTS)
    set_target_properties(alxbase PROPERTIES
        OUTPUT_NAME alxbase
        VERSION ${ALXLIB_VERSION}
        SOVERSION ${ALXLIB_SOVERSION}
    )
endif()

# Static library (base only, no dependencies)
if(ALXBASE_STATIC)
    add_library(alxbase_static STATIC $<TARGET_OBJECTS:alxbase_obj>)
    target_include_directories(alxbase_static PUBLIC ${ALXBASE_INCLUDE_DIRS})
    target_compile_features(alxbase_static PUBLIC cxx_std_17)
    target_compile_definitions(alxbase_static PRIVATE ALEXISLIB_STATIC=1)
    set_target_properties(alxbase_static PROPERTIES OUTPUT_NAME alxbase)
endif()
