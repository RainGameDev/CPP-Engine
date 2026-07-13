add_executable(engine main.cpp)

target_link_libraries(engine PRIVATE
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,editor_lib>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,core_lib>"
    glfw
)

target_include_directories(engine PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${stb_SOURCE_DIR}
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW glfw3)

add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:engine>/assets
)
