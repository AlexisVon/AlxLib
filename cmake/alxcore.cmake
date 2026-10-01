# alxcore - core extensions (file/compress/verify/thread + 3rdpty)

set(ALXCORE_SOURCES
    ${CMAKE_SOURCE_DIR}/source/alxcore/afile.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/averify.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/acompress.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/adatetime.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/athreadpool.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/afiber.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/aaes.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/astream_ex.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/afpacker.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/asqlite.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/acache.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/aplatform.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/aregex_pcre2.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/alogger.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcore/aimage.cpp
)

set(ALXCORE_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alxbase
    ${CMAKE_SOURCE_DIR}/include/alxcore
)

# 3rd party paths
set(ALXCORE_ZLIB_DIR ${CMAKE_SOURCE_DIR}/3rdpty/zlib)
set(ALXCORE_LZ4_DIR ${CMAKE_SOURCE_DIR}/3rdpty/lz4)
set(ALXCORE_ZSTD_DIR ${CMAKE_SOURCE_DIR}/3rdpty/zstd)
set(ALXCORE_SQLITE_DIR ${CMAKE_SOURCE_DIR}/3rdpty/sqlite3)
set(ALXCORE_JPEG_DIR ${CMAKE_SOURCE_DIR}/3rdpty/libjpeg-turbo64)
set(ALXCORE_PCRE2_DIR ${CMAKE_SOURCE_DIR}/3rdpty/pcre2)

set(ALXCORE_3RD_PARTY
    ${ALXCORE_ZLIB_DIR}/lib/libz.a
    ${ALXCORE_LZ4_DIR}/lib/liblz4.a
    ${ALXCORE_ZSTD_DIR}/lib/libzstd.a
    ${ALXCORE_SQLITE_DIR}/lib/libsqlite3.a
    ${ALXCORE_JPEG_DIR}/lib/libturbojpeg.a
    ${ALXCORE_PCRE2_DIR}/lib/libpcre2-8.a
)

# OBJECT library
if(ALXCORE_ENABLE OR ALXCORE_STATIC OR ALXLIB_STATIC)
    add_library(alxcore_obj OBJECT ${ALXCORE_SOURCES})
    target_include_directories(alxcore_obj PUBLIC ${ALXCORE_INCLUDE_DIRS})
    target_include_directories(alxcore_obj PRIVATE
        ${ALXCORE_ZLIB_DIR}/include
        ${ALXCORE_LZ4_DIR}/include
        ${ALXCORE_ZSTD_DIR}/include
        ${ALXCORE_SQLITE_DIR}/include
        ${ALXCORE_JPEG_DIR}/include
        ${ALXCORE_PCRE2_DIR}/include
    )
    target_compile_features(alxcore_obj PUBLIC cxx_std_17)
    set_target_properties(alxcore_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # SIMD support for crypto
    set_source_files_properties(${CMAKE_SOURCE_DIR}/source/alxcore/aaes.cpp PROPERTIES COMPILE_FLAGS "-maes -mpclmul -msse4.1")
    set_source_files_properties(${CMAKE_SOURCE_DIR}/source/alxcore/averify.cpp PROPERTIES COMPILE_FLAGS "-msha -msse4.1 -mcrc32")
endif()

# Shared library
if(ALXCORE_ENABLE)
    add_library(alxcore SHARED $<TARGET_OBJECTS:alxcore_obj>)
    target_include_directories(alxcore PUBLIC ${ALXCORE_INCLUDE_DIRS})
    target_compile_features(alxcore PUBLIC cxx_std_17)
    target_compile_definitions(alxcore PRIVATE ALXCORELIB_EXPORTS)
    target_link_libraries(alxcore PUBLIC alxbase)
    target_link_libraries(alxcore PRIVATE ${ALXCORE_3RD_PARTY})
    set_target_properties(alxcore PROPERTIES
        OUTPUT_NAME alxcore
        VERSION ${ALXLIB_VERSION}
        SOVERSION ${ALXLIB_SOVERSION}
    )
endif()

# Static library (core + base + 3rd party)
if(ALXCORE_STATIC)
    # Write 3rd party list for merge
    string(REPLACE ";" "\n" ALXCORE_3RD_PARTY_CONTENT "${ALXCORE_3RD_PARTY}")
    file(WRITE ${CMAKE_BINARY_DIR}/alxcore_3rdparty.txt "${ALXCORE_3RD_PARTY_CONTENT}")

    add_library(alxcore_static STATIC
        $<TARGET_OBJECTS:alxbase_obj>
        $<TARGET_OBJECTS:alxcore_obj>
    )
    target_include_directories(alxcore_static PUBLIC ${ALXCORE_INCLUDE_DIRS})
    target_compile_features(alxcore_static PUBLIC cxx_std_17)
    target_compile_definitions(alxcore_static PRIVATE ALEXISLIB_STATIC=1)
    set_target_properties(alxcore_static PROPERTIES OUTPUT_NAME alxcore)

    # Merge 3rd party
    add_custom_command(TARGET alxcore_static POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DALXLIB_ARCHIVE=$<TARGET_FILE:alxcore_static>
            -DTHIRD_PARTY_LIBS_FILE=${CMAKE_BINARY_DIR}/alxcore_3rdparty.txt
            -DAR=${CMAKE_AR}
            -P ${CMAKE_SOURCE_DIR}/cmake/merge_archive.cmake
        COMMENT "Merging 3rd party libraries into alxcore.a"
    )
endif()
