message(STATUS "Generating files for Nx Box Tool")

set(package_dir ${PACKAGES_DIR}/${rdep_target}/nx_box_tool-dev/zip)

set(stage_dir ${CMAKE_CURRENT_BINARY_DIR}/box_tool)
set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(build_dir ${CMAKE_CURRENT_BINARY_DIR}/build)
set(zip_file ${distribution_output_dir}/${box_tool_distribution_name}.zip)
set(zip_log ${logs_dir}/box_tool_zip.log)
set(nx_box_tool_binary ${build_dir}/dist/nx_box_tool)

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

if(targetDevice STREQUAL "linux-x64")
  # Command to be executed at the build phase.
  add_custom_command(OUTPUT ${zip_file}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${build_dir}
      COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage_dir} #< Delete old stage dir (if any).
      COMMAND ${CMAKE_COMMAND} -P ${copy_files_script} #< Create and populate sdk dir.

      COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/build.sh >${zip_log}
      COMMAND ${CMAKE_COMMAND} -E copy ${build_dir}/dist/nx_box_tool ${stage_dir}/
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/distrib/.nx_box_tool.ini ${stage_dir}/
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/distrib/nx_box_tool.conf ${stage_dir}/

      COMMAND ${CMAKE_COMMAND} -E echo
          "  Zipping SDK - see ${zip_log}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} #< Needed for zipping.
      COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip box_tool >${zip_log}

      ####COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage_dir}

      DEPENDS
          ${copy_files_script}
          ${copied_static_files}
  )
elseif(targetDevice STREQUAL "windows-x64")
  # Command to be executed at the build phase.
  add_custom_command(OUTPUT ${zip_file}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${build_dir}
      COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage_dir} #< Delete old stage dir (if any).
      COMMAND ${CMAKE_COMMAND} -P ${copy_files_script} #< Create and populate sdk dir.

      COMMAND cmd /c ${CMAKE_CURRENT_SOURCE_DIR}/build.bat >${zip_log}
      COMMAND ${CMAKE_COMMAND} -E copy ${build_dir}/dist/nx_box_tool.exe ${stage_dir}/
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/distrib/.nx_box_tool.ini ${stage_dir}/
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/distrib/nx_box_tool.conf ${stage_dir}/

      COMMAND ${CMAKE_COMMAND} -E echo
      "  Zipping SDK - see ${zip_log}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} #< Needed for zipping.
      COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip box_tool >${zip_log}

      ####COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage_dir}

      DEPENDS
          ${copy_files_script}
          ${copied_static_files}
  )
else()
    message(FATAL_ERROR Invalid targetDevice for building Nx Box Tool)
endif()

add_custom_target(nx_box_tool ALL DEPENDS ${zip_file})
