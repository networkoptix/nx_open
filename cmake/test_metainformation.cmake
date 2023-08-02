## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

set(testMetaInformationFile "${CMAKE_BINARY_DIR}/unit_tests_info.yml")
file(WRITE ${testMetaInformationFile} "")
nx_store_known_file(${testMetaInformationFile})

function(nx_store_test_metainformation target)
    set(oneValueArgs PROJECT COMPONENT EPIC)
    cmake_parse_arguments(NX_TEST_INFO
        #[[options]] ""
        "${oneValueArgs}"
        #[[multi_value_keywords]] ""
        ${ARGN})

    # If PROJECT was specified for the current module then using it.
    set(testProject ${NX_TEST_INFO_PROJECT})
    if(NOT testProject)
        # If PROJECT is not specified for the current module then using global value.
        set(testProject ${NX_TEST_JIRA_PROJECT})
    endif()

    if(NOT testProject)
        message(FATAL_ERROR
            "Add PROJECT parameter with a related Jira project to target ${target}")
    endif()

    set(epic ${NX_TEST_INFO_EPIC})
    if(NOT epic)
        # If EPIC is not specified for the current module then using predefined value.
        if(testProject STREQUAL "VMS")
            set(epic "VMS-40010")
        elseif(testProject STREQUAL "CB")
            set(epic "CB-354")
        endif()
    endif()

    set(requireComponent OFF)
    set(requireEpic OFF)
    if(NX_TEST_INFO_PROJECT STREQUAL "VMS")
        set(requireComponent ON)
        set(requireEpic ON)
    endif()
    if(NX_TEST_INFO_PROJECT STREQUAL "CB")
        set(requireEpic ON)
    endif()

    if(requireComponent AND NOT NX_TEST_INFO_COMPONENT)
        message(FATAL_ERROR
            "Add COMPONENT parameter with a related Jira component to target ${target}")
    endif()
    if(requireEpic AND NOT epic)
        message(FATAL_ERROR
            "Add EPIC parameter with a related Jira Epic to target ${target}")
    endif()

    file(APPEND ${testMetaInformationFile} "${target}: { project: ${testProject}")
    if(NX_TEST_INFO_COMPONENT)
        file(APPEND ${testMetaInformationFile} ", component: ${NX_TEST_INFO_COMPONENT}")
    endif()
    if(epic)
        file(APPEND ${testMetaInformationFile} ", epic: ${epic}")
    endif()
    file(APPEND ${testMetaInformationFile} " }\n")

endfunction()
