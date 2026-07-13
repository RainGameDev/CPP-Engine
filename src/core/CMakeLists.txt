file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

add_library(core_lib STATIC ${CORE_SOURCES})

target_include_directories(core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(core_lib PUBLIC
    ${stb_SOURCE_DIR}
)

target_link_libraries(core_lib PUBLIC
    glm::glm
    imgui
    nlohmann_json::nlohmann_json
    tinyobjloader
)

target_compile_definitions(core_lib PUBLIC VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1)
