set(BUILD_SHARED_LIBS ON)

set_output_directories(RUNTIME "bin/plugins" LIBRARY "bin/plugins")

add_subdirectory(genericrtspplugin)
add_subdirectory(image_library_plugin)
add_subdirectory(mjpg_link)
add_subdirectory(generic_multicast_plugin)
add_subdirectory(metadata)
if(LINUX AND box STREQUAL "none")
    add_subdirectory(it930x_plugin)
endif()
