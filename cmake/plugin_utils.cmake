## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

add_custom_target(server_plugins)
set_target_properties(server_plugins PROPERTIES FOLDER server/plugins)

set_property(GLOBAL PROPERTY nx_server_plugins_list)
set_property(GLOBAL PROPERTY nx_server_plugins_optional_list)

set(nx_add_plugin_to_distribution_if true) #< The default condition.

function(nx_add_server_plugin target)
    cmake_parse_arguments(PLUGIN "OPTIONAL;DEDICATED_DIR" "EXTRA_FILES_SUBDIR"
        "ADD_TO_DISTRIBUTION_IF;EXTRA_FILES" ${ARGN})

    if(PLUGIN_OPTIONAL)
        set(optionalPostfix "_optional")
    endif()

    if(PLUGIN_DEDICATED_DIR)
        set(dedicatedDirPathSuffix "/${target}")
    endif()

    string(CONCAT outputDirectory "bin/plugins${optionalPostfix}${dedicatedDirPathSuffix}")

    set_output_directories(RUNTIME "${outputDirectory}" LIBRARY "${outputDirectory}")

    if(PLUGIN_OPTIONAL)
        set_property(GLOBAL APPEND PROPERTY nx_server_plugins_optional_list ${target})
    else()
        set_property(GLOBAL APPEND PROPERTY nx_server_plugins_list ${target})
    endif()

    nx_add_target(${target}
        LIBRARY
        FOLDER server/plugins
        ${PLUGIN_UNPARSED_ARGUMENTS})

    if("${PLUGIN_ADD_TO_DISTRIBUTION_IF}" STREQUAL "")
        set(PLUGIN_ADD_TO_DISTRIBUTION_IF ${nx_add_plugin_to_distribution_if})
    endif()
    if(${PLUGIN_ADD_TO_DISTRIBUTION_IF})
        set_property(TARGET ${target} PROPERTY add_to_distribution ON)
    endif()

    if(PLUGIN_DEDICATED_DIR)
        set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()

    foreach(extra_file ${PLUGIN_EXTRA_FILES})
        if(PLUGIN_EXTRA_FILES_SUBDIR STREQUAL "")
            set(destination "${CMAKE_BINARY_DIR}/${outputDirectory}")
        else()
            set(destination "${CMAKE_BINARY_DIR}/${outputDirectory}/${PLUGIN_EXTRA_FILES_SUBDIR}")
        endif()
        nx_copy("${extra_file}" DESTINATION "${destination}" IF_DIFFERENT)
    endforeach()

    add_dependencies(server_plugins ${target})

    target_compile_definitions(${target}
        PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
        INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
    )
endfunction()
