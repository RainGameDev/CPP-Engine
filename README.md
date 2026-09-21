# Engine (name pending)

A 2D/3D game engine written in C++23, built for my personal projects. It's free, open-source, and runs on Linux and Windows.

> **Status**: Early development (v0.0.1). No stable version available. APIs and features are subject to change as they stabilise.

## Features

- Vulkan renderer with directional, point, and spot shadow support
- Entity system with gizmos, create, select, delete, and duplicate entities
- ImGui based editor with docking
- Jolt Physics integration (rigidbody and collider components)
- Asset loading: models (tinyobjloader), images (stb), shaders, and JSON scenes (nlohmann/json)
- Dependencies fetched via CMake FetchContent, no manual setup for most libraries

## Demo

![demo image](https://github.com/raingamedev/CPP-Engine/images/demo_image.png)

## Documentation

Documentation for variables, functions, structs, etc. is written inline as doc comments. Full written documentation will follow once features are finalised and their uses are fully known.

## Repository

- `main` is for active development of the next version.
- Releases are tagged `v.[major].[minor].[sub]`.
- Feature development branches use the name of the feature and are treated as experimental.

## Builds

Three shell scripts build and run the project:

| Script       | Target                                                                             |
| ------------ | ---------------------------------------------------------------------------------- |
| `run`        | Builds and runs the `engine` test target                                           |
| `run-editor` | Builds and runs the editor                                                         |
| `run-game`   | Builds and runs the game (will be removed when the projects system is implemented) |

All scripts default to a Debug build with Ninja into `build/`. They support a few environment overrides:

- `BUILD_TYPE=Release` - release build
- `CXX=g++` - use GCC instead of clang++
- `BUILD_DIR=<dir>` - custom build directory
- `GENERATOR="Unix Makefiles"` - custom CMake generator

## Compilation

Prerequisites:

- C++23 compiler (clang++ recommended, GCC works)
- CMake 3.22 or newer
- Ninja (or another generator)
- Vulkan SDK
- GLFW 3 installed system-wide, it's found via `find_package(glfw3)`, not fetched
- Network access on the first build, dependencies (glm, Dear ImGui, ImGuizmo, nlohmann/json, tinyobjloader, Jolt Physics) are downloaded via FetchContent

Linux:

```sh
./run-editor
```

Windows:

The build scripts are bash, so use WSL or Git Bash / MSYS2.

## Project structure

- `include/engine/` - public engine API
- `src/core/` - engine core
- `src/editor/` - editor application
- `src/game/` - game application
- `assets/` - models, materials, textures, shaders, and scenes
- `build/` - CMake build output (generated)

## Roadmap

- Projects system, a proper workflow that replaces the `run-game` script
- Full written documentation once features stabilise
- Possibly opening the project to contributions

## Contributing

Contributions are currently not accepted, this is a personal project. This may change in the future.

## License

Released under the [MIT License](LICENSE).
