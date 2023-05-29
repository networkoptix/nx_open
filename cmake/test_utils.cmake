## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

enable_testing()

add_custom_target(unit_tests)
set_target_properties(unit_tests PROPERTIES FOLDER utils)

set(testTempDirectory "${CMAKE_BINARY_DIR}" CACHE STRING "Temp directory for running tests.")
set(testMetaInformationFile "${CMAKE_BINARY_DIR}/unit_tests_info.yml")
file(WRITE ${testMetaInformationFile} "")
nx_store_known_file(${testMetaInformationFile})

function(nx_store_test_metainformation target)
    set(oneValueArgs PROJECT COMPONENT)
    cmake_parse_arguments(NX_TEST_INFO
        #[[options]] ""
        "${oneValueArgs}"
        #[[multi_value_keywords]] ""
        ${ARGN})

    if(NOT NX_TEST_INFO_PROJECT)
        message(FATAL_ERROR
            "Add PROJECT parameter with a related Jira project to target ${target}")
    endif()

    if(NX_TEST_INFO_COMPONENT)
        file(APPEND ${testMetaInformationFile}
            "${target}: { project: ${NX_TEST_INFO_PROJECT}, component: ${NX_TEST_INFO_COMPONENT} }\n")
    else()
        if(NX_TEST_INFO_PROJECT STREQUAL "VMS")
            message(FATAL_ERROR
                "Add COMPONENT parameter with a related Jira component to target ${target}")
        endif()
        file(APPEND ${testMetaInformationFile}
            "${target}: { project: ${NX_TEST_INFO_PROJECT} }\n")
    endif()

endfunction()

function(nx_add_test target) # [NO_GTEST] [NO_QT] [NO_NX_UTILS] ...
    set(options NO_GTEST NO_QT NO_NX_UTILS)
    set(oneValueArgs FOLDER PROJECT COMPONENT)
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
    endif()

    nx_add_target(${target}
        EXECUTABLE
        FORCE_INCLUDE
            ${force_include_header}
        NO_RC_FILE
        NO_MOC
        ${additional_private_libs}
        FOLDER ${folder}
        ${NX_ADD_TEST_UNPARSED_ARGUMENTS}
    )

    add_test(NAME ${target} COMMAND ${target})

    # Write unit test metainformation.
    nx_store_test_metainformation(${target}
        PROJECT ${NX_ADD_TEST_PROJECT}
        COMPONENT ${NX_ADD_TEST_COMPONENT})

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
