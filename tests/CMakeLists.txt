add_executable(engine_tests test_main.cpp)

target_link_libraries(engine_tests PRIVATE core_lib Catch2::Catch2WithMain)

target_include_directories(engine_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME engine_tests COMMAND engine_tests)
