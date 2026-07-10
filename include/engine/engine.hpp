#pragma once

#include <string>

namespace engine {

[[nodiscard]] auto greeting() -> std::string;
[[nodiscard]] auto version() -> std::string;

} // namespace engine
