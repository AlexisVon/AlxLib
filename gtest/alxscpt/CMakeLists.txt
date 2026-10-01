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

add_test(NAME alxscpt_test COMMAND alxscpt_test)
