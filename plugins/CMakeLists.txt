set(BUILD_SHARED_LIBS ON)
set_output_directories(RUNTIME "bin/plugins" LIBRARY "bin/plugins")

add_custom_target(server_plugins)

function(nx_add_server_plugin target)
    nx_add_target(${target} LIBRARY ${ARGN})
    add_dependencies(server_plugins ${target})
endfunction()

add_subdirectory(genericrtspplugin)
add_subdirectory(image_library_plugin)
add_subdirectory(mjpg_link)
add_subdirectory(generic_multicast_plugin)
add_subdirectory(metadata)
if(LINUX AND box STREQUAL "none")
    add_subdirectory(it930x_plugin)
endif()
