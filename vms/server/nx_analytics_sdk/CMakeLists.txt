# Create analytics_sdk folder at build phase.
# Build analytics_sdk zip, then build included sample(s) via included build_sample.sh/.bat at
# build phase.

include(find_doxygen)

set(depends mediaserver server_plugins) #< Not required technically, but useful to prevent errors.

message(STATUS "Generating files for Analytics SDK")

set(sdk_name analytics_sdk)
set(zip_file ${distribution_output_dir}/${analytics_sdk_distribution_name}.zip)
set(sdk_dir ${CMAKE_CURRENT_BINARY_DIR}/${sdk_name})
set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(sample_build_log ${logs_dir}/analytics_sdk_sample_build.log)
set(doxygen_log ${logs_dir}/analytics_sdk_doxygen.log)
set(zip_log ${logs_dir}/analytics_sdk_zip.log)

# Collect source files for statically prepared files.
set(copied_static_origin ${CMAKE_CURRENT_SOURCE_DIR}/${sdk_name})
file(GLOB_RECURSE copied_static_files ${copied_static_origin}/*)

# Collect source files for nx_kit.
set(copied_nx_kit_origin ${CMAKE_SOURCE_DIR}/open/artifacts/nx_kit)
file(GLOB_RECURSE copied_nx_kit_files ${copied_nx_kit_origin}/*)

# Collect source files for Plugin SDK.
set(copied_nx_sdk_src_dir ${CMAKE_SOURCE_DIR}/vms/libs/nx_sdk/src)
set(copied_nx_sdk_src_nx_origin ${copied_nx_sdk_src_dir}/nx)
file(GLOB_RECURSE copied_nx_sdk_src_nx_files ${copied_nx_sdk_src_nx_origin}/*)
set(copied_nx_sdk_old_header_origin ${copied_nx_sdk_src_dir}/plugins)
file(GLOB_RECURSE copied_nx_sdk_old_header_files ${copied_nx_sdk_old_header_origin}/*)

# Collect source files for the sample - Stub Analytics Plugin.
set(copied_sample_dir ${CMAKE_SOURCE_DIR}/vms/server/plugins/analytics/stub_analytics_plugin)
set(copied_sample_origin ${copied_sample_dir}/src)
file(GLOB_RECURSE copied_sample_files ${copied_sample_origin}/*)
set(copied_sample_ut_origin ${copied_sample_dir}/unit_tests/src)
file(GLOB_RECURSE copied_sample_ut_files ${copied_sample_ut_origin}/*)

set(create_sdk_script ${CMAKE_CURRENT_BINARY_DIR}/create_sdk.cmake)

# Substitute variable values into the cmake script which will be called at the build phase.
nx_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/create_sdk.cmake.in ${create_sdk_script} @ONLY)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(sample_build_script "./build_sample.sh")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(sample_build_script "build_sample.bat")
else()
    message(FATAL_ERROR "Building SDK sample(s) supported only for Windows and Linux.")
endif()

# On developer workstations, when not cross-compiling, unit tests for SDK samples will be run
# during the build phase. On CI, such tests are run with other tests, because they are included in
# unit test distribution.
if(NOT developerBuild OR NOT targetDevice MATCHES "linux-x64|windows-x64")
    set(sample_tests_trigger "--no-tests")
    set(cross_compiling_options
        -DPACKAGES_DIR=${PACKAGES_DIR}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    )
else()
    set(sample_tests_message " and running unit tests")
    set(cross_compiling_options "")
endif()

add_custom_command(OUTPUT ${zip_file}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${sdk_dir} #< Delete old sdk directory (if any).
    COMMAND ${CMAKE_COMMAND} -P ${create_sdk_script} #< Create and populate sdk directory.
    COMMAND ${CMAKE_COMMAND} -E echo
        "  Building samples${sample_tests_message} - see ${sample_build_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir} ${sample_build_script} ${sample_tests_trigger}
        ${cross_compiling_options}
        >${sample_build_log} 2>&1
    COMMAND ${CMAKE_COMMAND} -E echo
        "  Running Doxygen - see ${doxygen_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir} ${doxygen_executable} >${doxygen_log} 2>&1
    COMMAND ${CMAKE_COMMAND} -E echo
        "  Zipping SDK - see ${zip_log}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} #< Needed for zipping.
    COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip ${sdk_name} >${zip_log}
    DEPENDS
        ${create_sdk_script}
        ${CMAKE_CURRENT_SOURCE_DIR}/${sdk_name}/${sample_build_script}
        ${copied_static_files}
        ${copied_nx_kit_files}
        ${copied_nx_sdk_src_nx_files}
        ${copied_nx_sdk_old_header_files}
        ${copied_sample_files}
)

add_custom_target(nx_analytics_sdk ALL DEPENDS ${zip_file})
