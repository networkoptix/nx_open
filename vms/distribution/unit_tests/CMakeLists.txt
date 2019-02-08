if(WINDOWS)
    set(ext "zip")
else()
    set(ext "tar.gz")
endif()
set(package_file ${distribution_output_dir}/${unit_tests_distribution_name}.${ext})

nx_configure_file(build_distribution_conf.py.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_distribution_conf.py @ONLY)

set(depends unit_tests)
if(TARGET nx_analytics_sdk)
    list(APPEND depends nx_analytics_sdk)
endif()

set(script ${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.py)

set(python_path
    ${CMAKE_SOURCE_DIR}/build_utils/python
    ${CMAKE_CURRENT_BINARY_DIR}
)
if(NOT WINDOWS)
    string(REPLACE ";" ":" python_path "${python_path}")
endif()

add_custom_command(
    OUTPUT ${package_file}
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${python_path}"
        ${PYTHON_EXECUTABLE} ${script}
    DEPENDS ${script} ${depends} ${configured_files}
    COMMENT "Creating unit tests package ${package_file}"
    VERBATIM
)

add_custom_target(distribution_unit_tests ALL
    DEPENDS ${package_file}
)
