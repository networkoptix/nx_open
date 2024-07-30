## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Collect file lists to be included into the SDK at the generation phase via "configuring"
# (substituting file list variables into) a dedicated cmake script. Register a build-phase command
# that will zip the SDK directory and test that the SDK samples can be built via build scripts.

include(find_doxygen)
include(utils)

set(depends mediaserver server_plugins) #< Not required technically, but useful to prevent errors.

message(STATUS "Generating files for Metadata SDK")

set(sdk_name metadata_sdk)
set(zip_file ${distribution_output_dir}/${metadata_sdk_distribution_name}.zip)
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
set(copied_nx_sdk_src_nx_sdk_origin ${open_source_root}/vms/libs/nx_sdk/src/nx/sdk)
file(GLOB_RECURSE copied_nx_sdk_src_nx_sdk_files
    CONFIGURE_DEPENDS ${copied_nx_sdk_src_nx_sdk_origin}/*)
list(FILTER copied_nx_sdk_src_nx_sdk_files
    EXCLUDE REGEX "^${copied_nx_sdk_src_nx_sdk_origin}/cloud_storage/.*$")
set(copied_nx_sdk_unit_tests_origin ${open_source_root}/vms/libs/nx_sdk/unit_tests)
file(GLOB_RECURSE copied_nx_sdk_unit_tests_files CONFIGURE_DEPENDS
    ${copied_nx_sdk_unit_tests_origin}/*)
set(copied_nx_sdk_analytics_resources_origin
    ${open_source_root}/vms/libs/nx_plugin_utils/static-resources/analytics)
file(GLOB_RECURSE copied_nx_sdk_analytics_resources_files CONFIGURE_DEPENDS
    ${copied_nx_sdk_analytics_resources_origin}/*)

# Exclude settings_model.md and SDK documentation binary files (images) from SDK source files.
set(nx_sdk_src_settings_model_file ${copied_nx_sdk_src_nx_sdk_origin}/settings_model.md)
list(REMOVE_ITEM copied_nx_sdk_src_nx_sdk_files ${nx_sdk_src_settings_model_file})
file(GLOB_RECURSE nx_sdk_documentation_image_files CONFIGURE_DEPENDS
    ${copied_nx_sdk_src_nx_sdk_origin}/doc/*)
nx_subtract_lists(copied_nx_sdk_src_nx_sdk_files nx_sdk_documentation_image_files)

# Generate public version of settings_model.md.
set(copied_nx_sdk_settings_model_origin ${CMAKE_CURRENT_BINARY_DIR})
set(copied_nx_sdk_settings_model_file
    ${copied_nx_sdk_settings_model_origin}/settings_model.md)
nx_remove_proprietary_docs(${nx_sdk_src_settings_model_file} ${copied_nx_sdk_settings_model_file})

set(copied_samples_dir ${open_source_root}/vms/server/plugins/analytics)

# Collect source files for the sample 1 - Sample Analytics Plugin.
set(copied_sample_1_target sample_analytics_plugin)
set(copied_sample_1_origin ${copied_samples_dir}/${copied_sample_1_target})
file(GLOB_RECURSE copied_sample_1_files CONFIGURE_DEPENDS ${copied_sample_1_origin}/src/*)

# Collect source files for the sample 2 - Stub Analytics Plugin.
set(copied_sample_2_target stub_analytics_plugin)
set(copied_sample_2_origin ${copied_samples_dir}/${copied_sample_2_target})
file(GLOB_RECURSE copied_sample_2_files CONFIGURE_DEPENDS ${copied_sample_2_origin}/src/*
    ${copied_sample_2_origin}/object_streamer_files/*)

# Collect sample unit test files.
set(copied_sample_unit_tests_target unit_tests/src)
set(copied_sample_unit_tests_origin ${copied_samples_dir}/${copied_sample_unit_tests_target})
file(GLOB_RECURSE copied_sample_unit_tests_files CONFIGURE_DEPENDS
    ${copied_sample_unit_tests_origin}/*)

set(copy_sdk_files_script ${CMAKE_CURRENT_BINARY_DIR}/copy_sdk_files.cmake)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(sample_build_script "./build_samples.sh")
    set(build_scripts ${CMAKE_CURRENT_SOURCE_DIR}/files/${sample_build_script})
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(sample_build_script "build_samples.bat")
    set(msvc_environment_setup_script "call_vcvars64.bat")
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

# On developer workstations, when not cross-compiling, unit tests for SDK samples will be run
# during the build phase. On CI, such tests are run with other tests, because they are included in
# unit test distribution.

# We can run the SDK tests only if build platform equals target platform.
if(targetDevice STREQUAL ${default_target_device})
    # The tests will be built: inform about that.
    set(sample_tests_message " and running unit tests")
else()
    set(sampleTestsTrigger "--no-tests")
endif()

set(buildSamplesEnvironmentSetup "")
if(targetDevice STREQUAL "windows_x64")
    if(NOT openSourceBuild)
        # Use MSVC 2019 compiler for non-open-source builds only.
        set(buildSamplesEnvironmentSetup ${CMAKE_COMMAND} -E env NX_FORCED_MSVC_VERSION="14.29")
    endif()
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
nx_add_custom_command(OUTPUT ${zip_file}
    DEPENDS
        ${copy_sdk_files_script}
        ${build_scripts}
        ${copied_static_files}
        ${copied_nx_kit_files}
        ${copied_nx_sdk_src_nx_sdk_files}
        ${copied_nx_sdk_analytics_resources_files}
        ${copied_sample_1_files}
        ${copied_sample_2_files}
        ${copied_sample_unit_tests_files}
    BYPRODUCTS "${sdk_build_dir}" "${sdk_dir}"

    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -P ${copy_sdk_files_script} #< Create and populate sdk dir.

    COMMAND ${CMAKE_COMMAND} -E
        echo "  Building samples${sample_tests_message} - see ${sample_build_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir}
        ${buildSamplesEnvironmentSetup}
        ${sample_build_script} ${sampleTestsTrigger} ${compilingOptions}
        >${sample_build_log} 2>&1

    COMMAND ${CMAKE_COMMAND} -E
        echo "  Running Doxygen - see ${doxygen_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir}
        ${doxygen_executable} >${doxygen_log} 2>&1

    COMMAND ${CMAKE_COMMAND} -E
        echo "  Zipping SDK - see ${zip_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip ${sdk_name} >${zip_log}
)

add_custom_target(nx_metadata_sdk ALL DEPENDS ${zip_file})
set_target_properties(nx_metadata_sdk PROPERTIES FOLDER sdk)

# Save Metadata SDK build directory for usage in other places (e.g. building of the unit test
# distribution archive).
set(nx_metadata_sdk_build_dir "${sdk_build_dir}" CACHE INTERNAL "" FORCE)
