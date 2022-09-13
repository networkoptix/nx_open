## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Output:
#  - PIP_CMAKE_EXECUTABLE: variable in the parrent scope containing the path to the CMake
#    installed with pip.
function(_get_pip_cmake_executable)
    execute_process(COMMAND
        ${PYTHON_EXECUTABLE} ${open_source_root}/build_utils/python/find_cmake_from_pip_package.py
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE stderr_output
    )

    if(NOT result EQUAL 0)
        message(WARNING
            "Cannot find CMake executable installed by pip. Most likely it means that \"cmake\" "
            "pip package is not installed. For the detailed information, check the following "
            "message:\n"
            "${stderr_output}"
        )
    endif()

    string(STRIP "${output}" CMAKE_EXECUTABLE)
    set(PIP_CMAKE_EXECUTABLE ${CMAKE_EXECUTABLE} PARENT_SCOPE)
endfunction()

function(nx_prepare_vs_settings)
    # Create and initialize CMakeSettings.json if needed.
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/CMakeSettings.json")
        _get_pip_cmake_executable()
        if(PIP_CMAKE_EXECUTABLE)
            set(CMAKE_EXECUTABLE "${PIP_CMAKE_EXECUTABLE}")
        else()
            set(CMAKE_EXECUTABLE "${CMAKE_COMMAND}")
        endif()
        configure_file(
            "${PROJECT_SOURCE_DIR}/CMakeSettings.json.in"
            "${PROJECT_SOURCE_DIR}/CMakeSettings.json"
            @ONLY
        )
        message("Initial CMakeSettings.json created")
    endif()

    # Create .vs/launch.vs.json if needed.
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/.vs/launch.vs.json")
        file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/.vs")
        configure_file(
            "${PROJECT_SOURCE_DIR}/launch.vs.json.in"
            "${PROJECT_SOURCE_DIR}/.vs/launch.vs.json"
            @ONLY
        )
        message("Initial launch.vs.json created")
    endif()

    # Fix Qt path in CMakeSettings.json.
    execute_process(COMMAND
        ${PYTHON_EXECUTABLE} ${open_source_root}/build_utils/msvc/replace_conan_qt_path.py
            ${CMAKE_SOURCE_DIR}/CMakeSettings.json
            --qtdir=${CONAN_QT_ROOT}
            --build_root=${CMAKE_BINARY_DIR}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
    )
    if(NOT result EQUAL 0)
        message(WARNING "Qt path cannot be substituted to CMakeSettings.json: ${result}\n${output}")
    endif()
endfunction()
