# Create metadata_sdk folder at build phase.
# Build metadata_sdk zip, then build included sample(s) via included build_sample.sh/.bat at
# build phase.

include(find_doxygen)

set(depends mediaserver server_plugins) #< Not required technically, but useful to prevent errors.

message(STATUS "Generating files for Metadata SDK")

set(sdk_name metadata_sdk)
set(zip_file ${distribution_output_dir}/${metadata_sdk_distribution_name}.zip)
set(sdk_dir ${CMAKE_CURRENT_BINARY_DIR}/${sdk_name})
set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(sample_build_log ${logs_dir}/metadata_sdk_sample_build.log)
set(doxygen_log ${logs_dir}/metadata_sdk_doxygen.log)
set(zip_log ${logs_dir}/metadata_sdk_zip.log)

# Collect source files for statically prepared files.
set(sdk_src_static_origin ${CMAKE_CURRENT_SOURCE_DIR}/${sdk_name})
file(GLOB_RECURSE sdk_src_static_files ${sdk_src_static_origin}/*)

# Collect source files for nx_kit.
set(sdk_src_nx_kit_origin ${CMAKE_SOURCE_DIR}/open/artifacts/nx_kit)
file(GLOB_RECURSE sdk_src_nx_kit_files ${sdk_src_nx_kit_origin}/*)

# Collect source files for Plugin SDK.
set(sdk_src_nx_sdk_origin ${CMAKE_SOURCE_DIR}/common_libs/nx_sdk/src)
set(sdk_src_nx_sdk_new_origin ${sdk_src_nx_sdk_origin}/nx)
set(sdk_src_nx_sdk_common_headers_origin ${sdk_src_nx_sdk_origin}/plugins)
file(GLOB_RECURSE sdk_src_nx_sdk_new_files ${sdk_src_nx_sdk_new_origin}/*)
file(GLOB_RECURSE sdk_src_nx_sdk_common_headers ${sdk_src_nx_sdk_common_headers_origin}/*)

# Collect source files for the sample - Stub Metadata Plugin.
set(sdk_src_sample_origin ${CMAKE_SOURCE_DIR}/plugins/metadata/stub_metadata_plugin/src)
file(GLOB_RECURSE sdk_src_sample_files ${sdk_src_sample_origin}/*)

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

add_custom_command(OUTPUT ${zip_file}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${sdk_dir} #< Delete old sdk directory (if any).
    COMMAND ${CMAKE_COMMAND} -P ${create_sdk_script} #< Create and populate sdk directory.
    COMMAND ${CMAKE_COMMAND} -E echo
        "Building SDK samples - see ${sample_build_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir} ${sample_build_script}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DPACKAGES_DIR=${PACKAGES_DIR}
        >${sample_build_log} 2>&1
    COMMAND ${CMAKE_COMMAND} -E echo
        "Running Doxygen - see ${doxygen_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir} ${doxygen_executable} >${doxygen_log} 2>&1
    COMMAND ${CMAKE_COMMAND} -E echo
        "Zipping SDK - see ${zip_log}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} #< Needed for zipping.
    COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip ${sdk_name} >${zip_log}
    DEPENDS
        ${create_sdk_script}
        ${CMAKE_CURRENT_SOURCE_DIR}/${sdk_name}/${sample_build_script}
        ${sdk_src_static_files}
        ${sdk_src_nx_kit_files}
        ${sdk_src_nx_sdk_new_files}
        ${sdk_src_sample_files}
)

add_custom_target(nx_metadata_sdk ALL DEPENDS ${zip_file})
