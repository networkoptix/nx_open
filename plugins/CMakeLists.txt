set(BUILD_SHARED_LIBS ON)
set_output_directories(RUNTIME "bin/plugins" LIBRARY "bin/plugins")

if(WINDOWS)
    # Prepare a file to be included into each Visual Studio project.
    nx_configure_file(ut_msvc.user.props ${CMAKE_CURRENT_BINARY_DIR})
endif()

add_custom_target(server_plugins)

function(nx_add_server_plugin target)
    cmake_parse_arguments(PLUGIN "OPTIONAL" "" "" ${ARGN})

    if(PLUGIN_OPTIONAL)
        set_output_directories(RUNTIME "bin/plugins_optional" LIBRARY "bin/plugins_optional")
    endif()

    nx_add_target(${target} LIBRARY ${PLUGIN_UNPARSED_ARGUMENTS})
    add_dependencies(server_plugins ${target})
endfunction()

add_subdirectory(genericrtspplugin)

add_subdirectory(image_library_plugin)

add_subdirectory(mjpg_link)

add_subdirectory(generic_multicast_plugin)

add_subdirectory(metadata)

#add_subdirectory(webcam_plugin)

add_subdirectory(usb_cam)

if(LINUX AND box STREQUAL "none")
    add_subdirectory(it930x_plugin)
endif()
