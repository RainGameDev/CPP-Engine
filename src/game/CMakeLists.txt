add_executable(game main.cpp)

target_link_libraries(game PRIVATE
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,core_lib>"
    glfw
)

target_include_directories(game PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${stb_SOURCE_DIR}
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW glfw3)

add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:game>/assets
)
