# alxbase tests

find_package(GTest REQUIRED)

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(alxbase_test ${TEST_SOURCES})

target_include_directories(alxbase_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(alxbase_test PRIVATE
    alxbase
    GTest::GTest
    GTest::Main
)

add_test(NAME alxbase_test COMMAND alxbase_test)
