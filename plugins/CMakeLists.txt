set(BUILD_SHARED_LIBS ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins)

add_subdirectory(genericrtspplugin)
add_subdirectory(image_library_plugin)
add_subdirectory(mjpg_link)
add_subdirectory(it930x_plugin)
