# alxcomm tests

find_package(GTest REQUIRED)

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(alxcomm_test ${TEST_SOURCES})

target_include_directories(alxcomm_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alxbase
    ${CMAKE_SOURCE_DIR}/include/alxcore
    ${CMAKE_SOURCE_DIR}/include/alxcomm
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(alxcomm_test PRIVATE
    alxcomm
    GTest::GTest
)

add_test(NAME alxcomm_test COMMAND alxcomm_test)
