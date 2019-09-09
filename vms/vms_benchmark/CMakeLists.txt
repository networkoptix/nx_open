message(STATUS "Generating files for VMS Benchmark")

set(package_dir ${PACKAGES_DIR}/${rdep_target}/vms_benchmark-dev/zip)

set(stage_dir ${CMAKE_CURRENT_BINARY_DIR}/vms_benchmark)
set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(build_dir ${CMAKE_CURRENT_BINARY_DIR}/build)
set(zip_file ${distribution_output_dir}/${vms_benchmark_distribution_name}.zip)
set(zip_log ${logs_dir}/vms_benchmark_zip.log)
set(vms_benchmark_binary ${build_dir}/dist/vms_benchmark)

# Collect source files for statically prepared files.
set(copied_static_origin ${package_dir})
file(GLOB_RECURSE copied_static_files ${copied_static_origin}/*)

set(copy_files_script ${CMAKE_CURRENT_BINARY_DIR}/copy_files.cmake)
set(build_script_options ${CMAKE_CURRENT_BINARY_DIR}/build_options.conf)

# Substitute variable values into the cmake script which will be called at the build phase.
nx_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/copy_files.cmake.in
    ${copy_files_script} @ONLY)

nx_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build_options.conf.in
    ${build_script_options} @ONLY)

# Command to be executed at the build phase.
if(targetDevice STREQUAL "linux-x64")
    add_custom_command(OUTPUT ${zip_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${build_dir}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage_dir} #< Delete old stage dir (if any).
        COMMAND ${CMAKE_COMMAND} -P ${copy_files_script} #< Create and populate sdk dir.

        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/build.sh >${zip_log}
        COMMAND ${CMAKE_COMMAND} -E copy ${build_dir}/dist/vms_benchmark ${stage_dir}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/distrib/.vms_benchmark.ini ${stage_dir}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/distrib/vms_benchmark.conf ${stage_dir}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/readme.md ${stage_dir}/

        COMMAND ${CMAKE_COMMAND} -E echo
            "  Zipping VMS Benchmark - see ${zip_log}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} #< Needed for zipping.
        COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip vms_benchmark >${zip_log}

    ####COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage_dir}

        DEPENDS
            ${copy_files_script}
            ${copied_static_files}
    )
elseif(targetDevice STREQUAL "windows-x64")
    add_custom_command(OUTPUT ${zip_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${build_dir}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage_dir} #< Delete old stage dir (if any).
        COMMAND ${CMAKE_COMMAND} -P ${copy_files_script} #< Create and populate sdk dir.

        COMMAND cmd /c ${CMAKE_CURRENT_SOURCE_DIR}/build.bat >${zip_log}
        COMMAND ${CMAKE_COMMAND} -E copy ${build_dir}/dist/vms_benchmark.exe ${stage_dir}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/distrib/.vms_benchmark.ini ${stage_dir}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/distrib/vms_benchmark.conf ${stage_dir}/

        COMMAND ${CMAKE_COMMAND} -E echo
        "  Zipping VMS Benchmark - see ${zip_log}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} #< Needed for zipping.
        COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip vms_benchmark >${zip_log}

    ####COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage_dir}

        DEPENDS
            ${copy_files_script}
            ${copied_static_files}
  )
else()
    message(FATAL_ERROR "Unsupported targetDevice for building VMS Benchmark.")
endif()

add_custom_target(vms_benchmark ALL DEPENDS ${zip_file})
