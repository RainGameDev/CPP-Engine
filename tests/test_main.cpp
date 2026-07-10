#include <engine/engine.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("greeting returns expected string", "[engine]") {
    auto result = engine::greeting();
    REQUIRE(result == "Hello from engine v0.1.0");
}

TEST_CASE("version returns expected string", "[engine]") {
    auto result = engine::version();
    REQUIRE(result == "0.1.0");
}
