#include "engine/engine.hpp"

namespace engine {

auto greeting() -> std::string {
    return "Hello from engine v" + version();
}

auto version() -> std::string {
    return "0.1.0";
}

} // namespace engine
