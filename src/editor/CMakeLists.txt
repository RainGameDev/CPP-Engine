file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

list(FILTER EDITOR_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

add_library(editor_lib STATIC ${EDITOR_SOURCES})

target_include_directories(editor_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(editor_lib PUBLIC
    core_lib
    imgui
    imguizmo
)

add_executable(editor main.cpp)

target_link_libraries(editor PRIVATE
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,editor_lib>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,core_lib>"
    glfw
)

target_include_directories(editor PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${stb_SOURCE_DIR}
)

add_custom_command(TARGET editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:editor>/assets
)
