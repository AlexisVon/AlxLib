# alxscpt tests

find_package(GTest REQUIRED)

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(alxscpt_test ${TEST_SOURCES})

target_include_directories(alxscpt_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alxbase
    ${CMAKE_SOURCE_DIR}/include/alxcore
    ${CMAKE_SOURCE_DIR}/include/alxscpt
    ${CMAKE_SOURCE_DIR}/source/alxscpt
    ${CMAKE_SOURCE_DIR}/source/alxscpt/script
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(alxscpt_test PRIVATE
    alxscpt
    GTest::GTest
)

# Link fixtures whose load/create entry points throw: the tests pin that the failure is reported, not swallowed
add_library(alxscpt_test_link_load MODULE link/throw_load.cpp)
add_library(alxscpt_test_link_create MODULE link/throw_create.cpp)
# Fixtures for the yield service's link-facing contract: currying refuses in load/create, a dropped handle is inert
add_library(alxscpt_test_link_curry_load MODULE link/curry_load.cpp)
add_library(alxscpt_test_link_curry_create MODULE link/curry_create.cpp)
add_library(alxscpt_test_link_curry_mint MODULE link/curry_mint.cpp)
foreach(_link alxscpt_test_link_load alxscpt_test_link_create
              alxscpt_test_link_curry_load alxscpt_test_link_curry_create
              alxscpt_test_link_curry_mint)
    target_include_directories(${_link} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/alxbase
        ${CMAKE_SOURCE_DIR}/include/alxscpt
    )
endforeach()
target_compile_definitions(alxscpt_test PRIVATE
    ALX_TEST_LINK_LOAD_SO="$<TARGET_FILE:alxscpt_test_link_load>"
    ALX_TEST_LINK_CREATE_SO="$<TARGET_FILE:alxscpt_test_link_create>"
    ALX_TEST_LINK_CURRY_LOAD_SO="$<TARGET_FILE:alxscpt_test_link_curry_load>"
    ALX_TEST_LINK_CURRY_CREATE_SO="$<TARGET_FILE:alxscpt_test_link_curry_create>"
    ALX_TEST_LINK_CURRY_MINT_SO="$<TARGET_FILE:alxscpt_test_link_curry_mint>"
)
add_dependencies(alxscpt_test alxscpt_test_link_load alxscpt_test_link_create
                 alxscpt_test_link_curry_load alxscpt_test_link_curry_create
                 alxscpt_test_link_curry_mint)

add_test(NAME alxscpt_test COMMAND alxscpt_test)
