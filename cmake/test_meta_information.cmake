## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

function(nx_set_test_meta_info target)
    set(options SUITES)
    set(oneValueArgs PROJECT COMPONENT EPIC)
    cmake_parse_arguments(NX_TEST_INFO "${options}"
        "${oneValueArgs}" #[[multiValueArgs]] "" ${ARGN})

    # If PROJECT was specified for the current module then using it.
    set(project ${NX_TEST_INFO_PROJECT})
    if(NOT project)
        # If PROJECT is not specified for the current module then using global value.
        set(project ${NX_TEST_JIRA_PROJECT})
    endif()

    if(NOT project)
        message(FATAL_ERROR
            "Add PROJECT parameter with a related Jira project to target ${target}")
    endif()

    set(epic ${NX_TEST_INFO_EPIC})
    if(NOT epic)
        # If EPIC is not specified for the current module then using predefined value.
        if(project STREQUAL "VMS")
            set(epic "VMS-40010")
        elseif(project STREQUAL "CB")
            set(epic "CB-354")
        endif()
    endif()

    set(require_component OFF)
    set(require_epic OFF)
    set(suites_allowed OFF)
    if(NX_TEST_INFO_PROJECT STREQUAL "VMS")
        set(require_component ON)
        set(require_epic ON)
    endif()
    if(NX_TEST_INFO_PROJECT STREQUAL "CB")
        set(require_epic ON)
    endif()
    if(NX_TEST_INFO_SUITES)
        set(suites_allowed ON)
    endif()

    if(require_component AND NOT NX_TEST_INFO_COMPONENT)
        message(FATAL_ERROR
            "Add COMPONENT parameter with a related Jira component to target ${target}")
    endif()
    if(require_epic AND NOT epic)
        message(FATAL_ERROR
            "Add EPIC parameter with a related Jira Epic to target ${target}")
    endif()

    set_target_properties(${target} PROPERTIES NX_TEST_PROJECT ${project})
    set_target_properties(${target} PROPERTIES NX_TEST_SUITES ${suites_allowed})
    if(NX_TEST_INFO_COMPONENT)
        set_target_properties(${target} PROPERTIES NX_TEST_COMPONENT ${NX_TEST_INFO_COMPONENT})
    endif()
    if(epic)
        set_target_properties(${target} PROPERTIES NX_TEST_EPIC ${epic})
    endif()
endfunction()
