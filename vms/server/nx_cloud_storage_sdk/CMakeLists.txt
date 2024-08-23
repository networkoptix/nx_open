## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Collect file lists to be included into the SDK at the generation phase via "configuring"
# (substituting file list variables into) a dedicated cmake script. Register a build-phase command
# that will zip the SDK directory and test that the SDK samples can be built via build scripts.

include(find_doxygen)
include(utils)

set(depends mediaserver server_plugins) #< Not required technically, but useful to prevent errors.

message(STATUS "Generating files for Cloud Storage SDK")

set(sdk_name cloud_storage_sdk)
set(zip_file ${distribution_output_dir}/${cloud_storage_sdk_distribution_name}.zip)
set(server_resource_zip_file ${CMAKE_BINARY_DIR}/cloud_storage_sdk.zip)
set(sdk_dir ${CMAKE_CURRENT_BINARY_DIR}/${sdk_name})
set(sdk_build_dir "${sdk_dir}-build")
set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(sample_build_log ${logs_dir}/${sdk_name}_sample_build.log)
set(doxygen_log ${logs_dir}/${sdk_name}_doxygen.log)
set(zip_log ${logs_dir}/${sdk_name}_zip.log)

# Substitute nxSdkVersion into nx_sdk_version.inc to be included in nx/sdk/helpers/lib_context.cpp.
set(copied_nx_sdk_version_origin ${CMAKE_CURRENT_BINARY_DIR})
set(copied_nx_sdk_version_file ${copied_nx_sdk_version_origin}/nx_sdk_version.inc)
nx_configure_file(${open_source_root}/vms/libs/nx_sdk/src/nx_sdk_version.inc.in
    ${copied_nx_sdk_version_file} @ONLY NEWLINE_STYLE UNIX)

# Collect source files for statically prepared files.
set(copied_static_origin ${CMAKE_CURRENT_SOURCE_DIR}/files)
file(GLOB_RECURSE copied_static_files CONFIGURE_DEPENDS ${copied_static_origin}/*)

# Collect source files for nx_kit.
set(copied_nx_kit_origin ${open_source_root}/artifacts/nx_kit)
file(GLOB_RECURSE copied_nx_kit_files CONFIGURE_DEPENDS ${copied_nx_kit_origin}/*)
list(FILTER copied_nx_kit_files EXCLUDE REGEX "package-license.absent$")

# Collect source files for SDK.
set(copied_nx_sdk_src_origin ${open_source_root}/vms/libs/nx_sdk/src)
set(copied_nx_sdk_src_files
    ${copied_nx_sdk_src_origin}/plugins/plugin_api.h
    ${copied_nx_sdk_src_origin}/plugins/plugin_tools.h
    ${copied_nx_sdk_src_origin}/camera/camera_plugin.h
    ${copied_nx_sdk_src_origin}/camera/camera_plugin_types.h

    ${copied_nx_sdk_src_origin}/nx/sdk/ptr.h
    ${copied_nx_sdk_src_origin}/nx/sdk/dynamic_libraries.md
)
file(GLOB_RECURSE copied_nx_sdk_src_nx_sdk_files
    CONFIGURE_DEPENDS ${copied_nx_sdk_src_origin}/nx/sdk/*)
list(FILTER copied_nx_sdk_src_nx_sdk_files
    EXCLUDE REGEX "^${copied_nx_sdk_src_origin}/nx/sdk/analytics/.*$")
list(APPEND copied_nx_sdk_src_files ${copied_nx_sdk_src_nx_sdk_files})

# Exclude settings_model.md and SDK documentation binary files (images) from SDK source files.
set(nx_sdk_src_settings_model_file ${copied_nx_sdk_src_origin}/nx/sdk/settings_model.md)
list(REMOVE_ITEM copied_nx_sdk_src_files ${nx_sdk_src_settings_model_file})
file(GLOB_RECURSE nx_sdk_documentation_image_files CONFIGURE_DEPENDS
    ${copied_nx_sdk_src_origin}/nx/sdk/doc/*)
nx_subtract_lists(copied_nx_sdk_src_files nx_sdk_documentation_image_files)

# Generate public version of settings_model.md.
set(copied_nx_sdk_settings_model_origin ${CMAKE_CURRENT_BINARY_DIR})
set(copied_nx_sdk_settings_model_file
    ${copied_nx_sdk_settings_model_origin}/settings_model.md)
nx_remove_proprietary_docs(${nx_sdk_src_settings_model_file} ${copied_nx_sdk_settings_model_file})

set(copied_samples_dir ${open_source_root}/vms/server/plugins/cloud_storage)

# Collect source files for the sample 1 - Sample Cloud Storage Plugin.
set(copied_sample_1_target sample_cloud_storage_plugin)
set(copied_sample_1_origin ${copied_samples_dir}/${copied_sample_1_target})
file(GLOB_RECURSE copied_sample_1_files CONFIGURE_DEPENDS ${copied_sample_1_origin}/src/*)

# Collect source files for the sample 2 - Stub Cloud Storage Plugin.
set(copied_sample_2_target stub_cloud_storage_plugin)
set(copied_sample_2_origin ${copied_samples_dir}/${copied_sample_2_target})
file(GLOB_RECURSE copied_sample_2_files CONFIGURE_DEPENDS ${copied_sample_2_origin}/src/*)

set(copy_sdk_files_script ${CMAKE_CURRENT_BINARY_DIR}/copy_sdk_files.cmake)
set(msvc_environment_setup_script "call_vcvars64.bat")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(sample_build_script "./build_samples.sh")
    set(build_scripts ${CMAKE_CURRENT_SOURCE_DIR}/files/${sample_build_script})
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(sample_build_script "build_samples.bat")
    set(build_scripts
        ${CMAKE_CURRENT_SOURCE_DIR}/files/${sample_build_script}
        ${open_source_root}/build_utils/msvc/${msvc_environment_setup_script}
    )
else()
    message(FATAL_ERROR "Building SDK sample(s) supported only on Windows and Linux.")
endif()

# Substitute variable values into the cmake script which will be called at the build phase.
nx_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/copy_sdk_files.cmake.in
    ${copy_sdk_files_script} @ONLY)

    set(buildSamplesEnvironmentSetup "")
if(targetDevice STREQUAL "windows_x64")
    # Use MSVC 2019 compiler.
    set(buildSamplesEnvironmentSetup ${CMAKE_COMMAND} -E env NX_FORCED_MSVC_VERSION="14.29")
else()
    if(targetDevice STREQUAL "linux_arm32"
        OR targetDevice STREQUAL "edge1"
    )
        set(toolchainFile "linux_arm32")
    elseif(targetDevice STREQUAL "linux_arm64")
        set(toolchainFile "linux_arm64")
    elseif(targetDevice STREQUAL "linux_x64")
        set(toolchainFile "linux_x64")
    endif()

    set(compilingOptions -DCONAN_SDK-GCC_ROOT=${CONAN_SDK-GCC_ROOT})

    if(toolchainFile)
        set(compilingOptions ${compilingOptions}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/toolchains/${toolchainFile}.cmake
        )
    endif()
endif()

# Command to be executed at the build phase.
nx_add_custom_command(OUTPUT ${zip_file} ${server_resource_zip_file}
    DEPENDS
        ${copy_sdk_files_script}
        ${build_scripts}
        ${copied_static_files}
        ${copied_nx_kit_files}
        ${copied_nx_sdk_src_files}
        ${copied_sample_1_files}
        ${copied_sample_2_files}
    BYPRODUCTS "${sdk_build_dir}" "${sdk_dir}"

    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -P ${copy_sdk_files_script} #< Create and populate sdk dir.

    COMMAND ${CMAKE_COMMAND} -E
        echo "  Building samples - see ${sample_build_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir}
        ${buildSamplesEnvironmentSetup}
        ${sample_build_script} ${compilingOptions}
        >${sample_build_log} 2>&1

    COMMAND ${CMAKE_COMMAND} -E
        echo "  Running Doxygen - see ${doxygen_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir}
        ${doxygen_executable} >${doxygen_log} 2>&1

    COMMAND ${CMAKE_COMMAND} -E
        echo "  Zipping SDK - see ${zip_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip ${sdk_name} >${zip_log}

    COMMAND ${CMAKE_COMMAND} -E copy ${zip_file} ${server_resource_zip_file}
)

add_custom_target(nx_cloud_storage_sdk ALL DEPENDS ${zip_file})
set_target_properties(nx_cloud_storage_sdk PROPERTIES FOLDER sdk)
