## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

enable_testing()

include(test_meta_information)

set(testInformationFile "${CMAKE_BINARY_DIR}/unit_tests_info.yml")
nx_store_known_file(${testInformationFile})
set(testTempDirectory "${CMAKE_BINARY_DIR}" CACHE STRING "Temp directory for running tests.")

add_custom_target(unit_tests)
set_target_properties(unit_tests PROPERTIES FOLDER utils)

function(nx_add_test target) # [NO_GTEST] [NO_QT] [NO_NX_UTILS] ...
    if(NOT withTests)
        message(FATAL_ERROR "Function nx_add_test can be used only with enabled withTests.")
    endif()

    set(options NO_GTEST NO_QT NO_NX_UTILS)
    set(oneValueArgs FOLDER PROJECT COMPONENT EPIC)
    cmake_parse_arguments(NX_ADD_TEST "${options}"
        "${oneValueArgs}" #[[multi_value_keywords]] "" ${ARGN})

    set_output_directories(RUNTIME "bin" LIBRARY "lib"
        AFFECTED_VARIABLES_RESULT affected_variables)
    nx_expose_variables_to_parent_scope(${affected_variables})

    if(NOT NX_ADD_TEST_NO_GTEST)
        set(additional_private_libs
            PRIVATE_LIBS
                GTest
                GMock
        )
    endif()

    if(targetDevice STREQUAL "linux_arm32")
        # Linux for ARM32 expects ffmpeg to be located in "../lib/ffmpeg" directory.
        string(JOIN ":" CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} "$ORIGIN/../lib/ffmpeg")
    endif()

    if(NX_ADD_TEST_FOLDER)
        set(folder "${NX_ADD_TEST_FOLDER}")
    else()
        set(folder "tests")
    endif()

    if(NOT NX_ADD_TEST_NO_GTEST AND NOT NX_ADD_TEST_NO_NX_UTILS AND NOT NX_ADD_TEST_NO_QT)
        set(force_include_header
            ${nx_utils_dir}/src/nx/utils/test_support/custom_gtest_printers.h)
        set(additional_test_support_private_libs
            PRIVATE_LIBS
                nx_utils
                Qt6::Core)
    endif()

    nx_add_target(${target}
        EXECUTABLE
        FORCE_INCLUDE
            ${force_include_header}
        NO_RC_FILE
        NO_MOC
        ${additional_private_libs}
        ${additional_test_support_private_libs}
        FOLDER ${folder}
        ${NX_ADD_TEST_UNPARSED_ARGUMENTS}
    )

    add_test(NAME ${target} COMMAND ${target})

    nx_set_test_meta_info(${target}
        PROJECT ${NX_ADD_TEST_PROJECT}
        COMPONENT ${NX_ADD_TEST_COMPONENT}
        EPIC ${NX_ADD_TEST_EPIC})

    if(WINDOWS)
        # Adding ${CMAKE_MSVCIDE_RUN_PATH} to PATH for running unit tests via CTest.
        set(new_path "${CMAKE_MSVCIDE_RUN_PATH};$ENV{PATH}")
        string(REPLACE ";" "\;" new_path "${new_path}")
        set_tests_properties(${target} PROPERTIES ENVIRONMENT "PATH=${new_path}")
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE OFF) #< Build a console app.
    endif()

    add_dependencies(unit_tests ${target})
endfunction()

function(nx_add_server_plugin_test target) # [NO_GTEST] [NO_QT]
    if(MACOSX)
        set(CMAKE_INSTALL_RPATH
            "@executable_path/../lib"
            "@executable_path/plugins"
            "@executable_path/plugins_optional")
    else()
        set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN/plugins:$ORIGIN/plugins_optional")
    endif()

    list(APPEND CMAKE_MSVCIDE_RUN_PATH
        ${CMAKE_BINARY_DIR}/bin/plugins
        ${CMAKE_BINARY_DIR}/bin/plugins_optional
    )

    nx_add_test(${ARGV}
        PROJECT VMS
        COMPONENT Server)

    if(WINDOWS)
        # Include "ut_msvc.user.props" into each Visual Studio project.
        set_target_properties(${target} PROPERTIES
            VS_USER_PROPS ${CMAKE_BINARY_DIR}/vms/server/plugins/ut_msvc.user.props)
    endif()
endfunction()

function(nx_dump_unit_tests)
    get_target_property(tests unit_tests MANUALLY_ADDED_DEPENDENCIES)

    if(NOT tests)
        return()
    endif()

    set(content)

    foreach(test IN LISTS tests)
        string(APPEND content "${test}:\n")

        get_target_property(project ${test} NX_TEST_PROJECT)
        if(NOT project)
            message(FATAL_ERROR
                "NX_TEST_PROJECT property is not set for test ${test}.  "
                "Check if the meta information was set properly with nx_set_test_meta_info().")
        endif()
        string(APPEND content "  project: ${project}\n")
        get_target_property(component ${test} NX_TEST_COMPONENT)
        if(component)
            string(APPEND content "  component: ${component}\n")
        endif()
        get_target_property(epic ${test} NX_TEST_EPIC)
        if(epic)
            string(APPEND content "  epic: ${epic}\n")
        endif()

        set(binary_dependencies)
        nx_get_target_manual_dependencies(${test} dependencies)
        foreach(dependency IN LISTS dependencies)
            get_target_property(type ${dependency} TYPE)
            get_target_property(file ${dependency} OUTPUT_FILE)
            if(type STREQUAL EXECUTABLE OR file)
                list(APPEND binary_dependencies ${dependency})
            endif()
        endforeach()

        if(binary_dependencies)
            string(APPEND content "  binary_dependencies:\n")
            foreach(dependency IN LISTS binary_dependencies)
                string(APPEND content "    - ${dependency}\n")
            endforeach()
        endif()
    endforeach()

    file(WRITE ${testInformationFile} ${content})
endfunction()

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL nx_dump_unit_tests)
