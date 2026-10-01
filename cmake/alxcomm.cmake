# alxcomm - communication module

set(ALXCOMM_SOURCES
    ${CMAKE_SOURCE_DIR}/source/alxcomm/acomm.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcomm/acomm_ex.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcomm/ahttp.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcomm/arpc.cpp
    ${CMAKE_SOURCE_DIR}/source/alxcomm/atransmit.cpp
)

set(ALXCOMM_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alxbase
    ${CMAKE_SOURCE_DIR}/include/alxcore
    ${CMAKE_SOURCE_DIR}/include/alxcomm
)

set(ALXCOMM_OPENSSL_DIR ${CMAKE_SOURCE_DIR}/3rdpty/openssl)

# OBJECT library
if(ALXCOMM_ENABLE OR ALXCOMM_STATIC OR ALXLIB_STATIC)
    add_library(alxcomm_obj OBJECT ${ALXCOMM_SOURCES})
    target_include_directories(alxcomm_obj PUBLIC ${ALXCOMM_INCLUDE_DIRS})
    target_compile_features(alxcomm_obj PUBLIC cxx_std_17)
    set_target_properties(alxcomm_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

    if(ALXCOMM_TLS)
        target_include_directories(alxcomm_obj PRIVATE ${ALXCOMM_OPENSSL_DIR}/include)
        target_compile_definitions(alxcomm_obj PRIVATE ALEXIS_USE_TLS=1)
    endif()
endif()

# Shared library
if(ALXCOMM_ENABLE)
    add_library(alxcomm SHARED $<TARGET_OBJECTS:alxcomm_obj>)
    target_include_directories(alxcomm PUBLIC ${ALXCOMM_INCLUDE_DIRS})
    target_compile_features(alxcomm PUBLIC cxx_std_17)
    target_compile_definitions(alxcomm PRIVATE ALXCOMMLIB_EXPORTS)
    target_link_libraries(alxcomm PUBLIC alxbase alxcore)
    if(ALXCOMM_TLS)
        target_link_libraries(alxcomm PRIVATE
            ${ALXCOMM_OPENSSL_DIR}/lib/libssl.a
            ${ALXCOMM_OPENSSL_DIR}/lib/libcrypto.a
        )
    endif()
    set_target_properties(alxcomm PROPERTIES
        OUTPUT_NAME alxcomm
        VERSION ${ALXLIB_VERSION}
        SOVERSION ${ALXLIB_SOVERSION}
    )
endif()

# Static library (comm + core + base + 3rd party + optional openssl)
if(ALXCOMM_STATIC)
    # Core's 3rd party + optional openssl
    set(ALXCOMM_3RD_PARTY ${ALXCORE_3RD_PARTY})
    if(ALXCOMM_TLS)
        list(APPEND ALXCOMM_3RD_PARTY
            ${ALXCOMM_OPENSSL_DIR}/lib/libssl.a
            ${ALXCOMM_OPENSSL_DIR}/lib/libcrypto.a
        )
    endif()
    string(REPLACE ";" "\n" ALXCOMM_3RD_PARTY_CONTENT "${ALXCOMM_3RD_PARTY}")
    file(WRITE ${CMAKE_BINARY_DIR}/alxcomm_3rdparty.txt "${ALXCOMM_3RD_PARTY_CONTENT}")

    add_library(alxcomm_static STATIC
        $<TARGET_OBJECTS:alxbase_obj>
        $<TARGET_OBJECTS:alxcore_obj>
        $<TARGET_OBJECTS:alxcomm_obj>
    )
    target_include_directories(alxcomm_static PUBLIC ${ALXCOMM_INCLUDE_DIRS})
    target_compile_features(alxcomm_static PUBLIC cxx_std_17)
    target_compile_definitions(alxcomm_static PRIVATE ALEXISLIB_STATIC=1)
    set_target_properties(alxcomm_static PROPERTIES OUTPUT_NAME alxcomm)

    # Merge 3rd party
    add_custom_command(TARGET alxcomm_static POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DALXLIB_ARCHIVE=$<TARGET_FILE:alxcomm_static>
            -DTHIRD_PARTY_LIBS_FILE=${CMAKE_BINARY_DIR}/alxcomm_3rdparty.txt
            -DAR=${CMAKE_AR}
            -P ${CMAKE_SOURCE_DIR}/cmake/merge_archive.cmake
        COMMENT "Merging 3rd party libraries into alxcomm.a"
    )
endif()
