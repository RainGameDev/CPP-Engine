file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/include/*.h
    ${CMAKE_SOURCE_DIR}/include/*.hpp
)
list(REMOVE_ITEM ENGINE_SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)
add_library(engine_lib STATIC ${ENGINE_SOURCES})
target_include_directories(engine_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(engine_lib PRIVATE ${CMAKE_SOURCE_DIR}/src ${stb_SOURCE_DIR})
target_link_libraries(engine_lib PUBLIC glm::glm imgui nlohmann_json::nlohmann_json)
target_compile_definitions(engine_lib PUBLIC VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1)

add_executable(engine main.cpp)
target_link_libraries(engine PRIVATE engine_lib glfw)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW glfw3)

add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:engine>/assets
)
