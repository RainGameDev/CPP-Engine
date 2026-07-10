add_executable(engine_tests test_main.cpp)

target_link_libraries(engine_tests PRIVATE engine_lib Catch2::Catch2WithMain)

add_test(NAME engine_tests COMMAND engine_tests)
