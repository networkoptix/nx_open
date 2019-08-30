message(STATUS "Generating files for Nx Box Tool")

set(package_dir ${PACKAGES_DIR}/${rdep_target}/nx_box_tool-dev/zip)

set(stage_dir ${CMAKE_CURRENT_BINARY_DIR}/box_tool)
set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(zip_file ${distribution_output_dir}/${box_tool_distribution_name}.zip)
set(zip_log ${logs_dir}/box_tool_zip.log)

# Collect source files for statically prepared files.
set(copied_static_origin ${package_dir})
file(GLOB_RECURSE copied_static_files ${copied_static_origin}/*)

set(copy_files_script ${CMAKE_CURRENT_BINARY_DIR}/copy_files.cmake)

# Substitute variable values into the cmake script which will be called at the build phase.
nx_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/copy_files.cmake.in
    ${copy_files_script} @ONLY)

# Command to be executed at the build phase.
add_custom_command(OUTPUT ${zip_file}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage_dir} #< Delete old stage dir (if any).
    COMMAND ${CMAKE_COMMAND} -P ${copy_files_script} #< Create and populate sdk dir.

    COMMAND ${CMAKE_COMMAND} -E echo
        "  Zipping SDK - see ${zip_log}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} #< Needed for zipping.
    COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip box_tool >${zip_log}

####COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage_dir}

    DEPENDS
        ${copy_files_script}
        ${copied_static_files}
)

add_custom_target(nx_box_tool ALL DEPENDS ${zip_file})
