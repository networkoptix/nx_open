nx_configure_file(${customization_dir}/license.html
    ${distribution_output_dir}/eula-${eulaVersion}.html)

function(get_distributable_plugins_list variable_name property_name)
    get_property(plugins GLOBAL PROPERTY ${property_name})

    set(result)
    foreach(plugin ${plugins})
        get_property(distributable TARGET ${plugin} PROPERTY add_to_distribution)
        if(distributable)
            list(APPEND result ${plugin})
        endif()
    endforeach()

    string(REPLACE ";" " " result "${result}")

    set(${variable_name} ${result} PARENT_SCOPE)
endfunction()

# These variables are to be substituted into the config files, so they are space-separated lists.
get_distributable_plugins_list(server_plugins_list nx_server_plugins_list)
get_distributable_plugins_list(server_plugins_optional_list nx_server_plugins_optional_list)

if(WINDOWS)
    add_subdirectory(wix)
elseif(LINUX AND (
    box STREQUAL "none" OR
    arch STREQUAL "arm64" OR
    targetDevice STREQUAL "linux_arm32"
)) #< Desktop or generic Arm Linux device.
    add_subdirectory(deb)
elseif(MACOSX)
    add_subdirectory(dmg)
elseif(targetDevice MATCHES "bpi|edge1")
    add_subdirectory(tar)
endif()

if(withUnitTestsArchive)
    add_subdirectory(unit_tests)
endif()
