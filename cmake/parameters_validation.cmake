set(skipConfigurationChecks "" CACHE STRING "")
mark_as_advanced(skipConfigurationChecks)

function(nx_fail_configuration_check check_id)
    cmake_parse_arguments(FAIL "" "DESCRIPTION" "PRINT_VARIABLES" ${ARGN})

    set(message "Configuration check failed: ${check_id}\n")
    string(APPEND message ${FAIL_DESCRIPTION})

    if(NOT "${FAIL_PRINT_VARIABLES}" STREQUAL "")
        string(APPEND message "\nConflicting variables:")
        foreach(var ${FAIL_PRINT_VARIABLES})
            string(APPEND message "\n  ${var} = ${${var}}")
        endforeach()
    endif()

    message(FATAL_ERROR ${message})
endfunction()

if(NOT developerBuild AND NOT dw_edge1 IN_LIST skipConfigurationChecks)
    if(targetDevice STREQUAL "edge1" AND NOT customization STREQUAL "digitalwatchdog")
        nx_fail_configuration_check(dw_edge1
            DESCRIPTION "edge1 can be used only with digitalwatchdog customization."
            PRINT_VARIABLES targetDevice customization)
    endif()
endif()

if(NOT developerBuild AND NOT customizations_for_nx1 IN_LIST skipConfigurationChecks)
    set(_nx1_customizations default default_cn default_zh_CN vista)
    if(targetDevice STREQUAL "bpi" AND NOT customization IN_LIST _nx1_customizations)
        string(REPLACE ";" ", " _nx1_customizations "${_nx1_customizations}")
        nx_fail_configuration_check(customizations_for_nx1
            DESCRIPTION "${targetDevice} can be used only with the following customizations: ${_nx1_customizations}."
            PRINT_VARIABLES targetDevice customization)
    endif()
endif()

if(hardwareSigning AND NOT hardware_signing IN_LIST skipConfigurationChecks)
    if(WINDOWS AND NOT customization STREQUAL "vista")
        nx_fail_configuration_check(hardware_signing
            DESCRIPTION "hardwareSigning can be used only with vista customization."
            PRINT_VARIABLES hardwareSigning customization)
    endif()
endif()
