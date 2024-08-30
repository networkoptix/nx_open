## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(WINDOWS)
    set(ext "zip")
else()
    set(ext "tar.gz")
endif()
set(package_file ${distribution_output_dir}/${unit_tests_distribution_name}.${ext})

nx_configure_file(build_distribution_conf.py.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution_conf.py @ONLY)

set(depends unit_tests)
if(TARGET nx_server_plugin_sdk)
    list(APPEND depends nx_server_plugin_sdk)
endif()
if(TARGET nx_sdk_ut)
    list(APPEND depends nx_sdk_ut)
endif()

set(script ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.py)

set(python_path
    ${open_source_root}/build_utils/distribution
    ${CMAKE_CURRENT_BINARY_DIR}
)
if(NOT WINDOWS)
    string(REPLACE ";" ":" python_path "${python_path}")
endif()

get_target_property(testTargetList unit_tests TESTS)
message(STATUS "Ready to build unit tests archive out of ${testTargetList}")

add_custom_command(
    OUTPUT ${package_file}
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${python_path}"
        ${PYTHON_EXECUTABLE} ${script}
    DEPENDS
        ${script}
        ${depends}
        ${configured_files}
        "${CMAKE_BINARY_DIR}/${NX_AFFECTED_TARGETS_LIST_FILE_NAME}"
    COMMENT "Creating unit tests package ${package_file}"
    VERBATIM
)

add_custom_target(distribution_unit_tests ALL
    DEPENDS ${package_file}
)
set_target_properties(distribution_unit_tests PROPERTIES FOLDER distribution)
