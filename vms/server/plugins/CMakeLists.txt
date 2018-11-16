set(BUILD_SHARED_LIBS ON)
set_output_directories(RUNTIME "bin/plugins" LIBRARY "bin/plugins")

if(WINDOWS)
    # Prepare a file to be included into each Visual Studio project.
    nx_configure_file(ut_msvc.user.props ${CMAKE_CURRENT_BINARY_DIR})
endif()

add_custom_target(server_plugins)

set(nx_add_plugin_to_distribution_if NOT targetDevice STREQUAL "edge1")
set_property(GLOBAL PROPERTY nx_server_plugins_list)
set_property(GLOBAL PROPERTY nx_server_plugins_optional_list)

function(nx_add_server_plugin target)
    cmake_parse_arguments(PLUGIN "OPTIONAL" "" "ADD_TO_DISTRIBUTION_IF" ${ARGN})

    if(PLUGIN_OPTIONAL)
        set_output_directories(RUNTIME "bin/plugins_optional" LIBRARY "bin/plugins_optional")
        set_property(GLOBAL APPEND PROPERTY nx_server_plugins_optional_list ${target})
    else()
        set_property(GLOBAL APPEND PROPERTY nx_server_plugins_list ${target})
    endif()

    nx_add_target(${target} LIBRARY ${PLUGIN_UNPARSED_ARGUMENTS})

    if("${PLUGIN_ADD_TO_DISTRIBUTION_IF}" STREQUAL "")
        set(PLUGIN_ADD_TO_DISTRIBUTION_IF ${nx_add_plugin_to_distribution_if})
    endif()
    if(${PLUGIN_ADD_TO_DISTRIBUTION_IF})
        set_property(TARGET ${target} PROPERTY add_to_distribution ON)
    endif()

    add_dependencies(server_plugins ${target})

    target_compile_definitions(${target}
        PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
        INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
    )
endfunction()

add_subdirectory(analytics)

add_subdirectory(device)

