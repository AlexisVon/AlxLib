# alxcore tests

find_package(GTest REQUIRED)

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(alxcore_test ${TEST_SOURCES})

target_include_directories(alxcore_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alxbase
    ${CMAKE_SOURCE_DIR}/include/alxcore
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(alxcore_test PRIVATE
    alxcore
    GTest::GTest
)

add_test(NAME alxcore_test COMMAND alxcore_test)
