#include "application.h"
#include <cstdlib>
#include <exception>
#include <print>

int main() {
  try {
    Application app;
    app.setEditorMode(true);
    app.run();
  } catch (const std::exception &e) {
    std::print("Err: {}\n", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
