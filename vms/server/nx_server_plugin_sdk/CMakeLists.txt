## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Collect file lists to be included into the SDK at the generation phase via "configuring"
# (substituting file list variables into) a dedicated cmake script. Register a build-phase command
# that will zip the SDK directory and test that the SDK samples can be built via build scripts.

include(find_doxygen)
include(utils)

set(depends "")
set(sdk_example_files_json "[]")

macro(_collect_nx_sdk_file_names)
    # Substitute nxSdkVersion into nx_sdk_version.inc to be included in
    # nx/sdk/helpers/lib_context.cpp.
    set(copied_nx_sdk_version_origin ${CMAKE_CURRENT_BINARY_DIR})
    set(copied_nx_sdk_version_file ${copied_nx_sdk_version_origin}/nx_sdk_version.inc)
    nx_configure_file(${open_source_root}/vms/libs/nx_sdk/src/nx_sdk_version.inc.in
        ${copied_nx_sdk_version_file} @ONLY NEWLINE_STYLE UNIX)

    # Collect source files for statically prepared files.
    set(copied_static_origin ${CMAKE_CURRENT_SOURCE_DIR}/files)
    file(GLOB_RECURSE copied_static_files CONFIGURE_DEPENDS ${copied_static_origin}/*)
    list(APPEND depends ${copied_static_files})

    # Collect source files for nx_kit.
    set(copied_nx_kit_origin ${open_source_root}/artifacts/nx_kit)
    file(GLOB_RECURSE copied_nx_kit_files CONFIGURE_DEPENDS ${copied_nx_kit_origin}/*)
    list(FILTER copied_nx_kit_files EXCLUDE REGEX "package-license.absent$")
    list(APPEND depends ${copied_nx_kit_files})

    # Collect source files for SDK.
    set(copied_nx_sdk_src_origin ${open_source_root}/vms/libs/nx_sdk/src)
    set(copied_nx_sdk_src_nx_sdk_origin ${copied_nx_sdk_src_origin}/nx/sdk)
    file(GLOB_RECURSE copied_nx_sdk_src_nx_sdk_files
        CONFIGURE_DEPENDS ${copied_nx_sdk_src_nx_sdk_origin}/*)
    set(copied_nx_sdk_src_additional_files
        ${copied_nx_sdk_src_origin}/plugins/plugin_api.h
        ${copied_nx_sdk_src_origin}/plugins/plugin_tools.h
        ${copied_nx_sdk_src_origin}/camera/camera_plugin.h
        ${copied_nx_sdk_src_origin}/camera/camera_plugin_types.h
        ${copied_nx_sdk_src_origin}/storage/third_party_storage.h
    )
    list(APPEND copied_nx_sdk_src_nx_sdk_files ${copied_nx_sdk_src_additional_files})

    set(copied_nx_sdk_unit_tests_origin ${open_source_root}/vms/libs/nx_sdk/unit_tests)
    file(GLOB_RECURSE copied_nx_sdk_unit_tests_files CONFIGURE_DEPENDS
        ${copied_nx_sdk_unit_tests_origin}/*)
    set(copied_nx_sdk_analytics_resources_origin
        ${open_source_root}/vms/libs/nx_plugin_utils/static-resources/analytics)
    file(GLOB_RECURSE copied_nx_sdk_analytics_resources_files CONFIGURE_DEPENDS
        ${copied_nx_sdk_analytics_resources_origin}/*)
    list(APPEND depends ${copied_nx_sdk_analytics_resources_files})

    # Exclude settings_model.md and SDK documentation binary files (images) from SDK source files.
    set(nx_sdk_src_settings_model_file ${copied_nx_sdk_src_nx_sdk_origin}/settings_model.md)
    list(REMOVE_ITEM copied_nx_sdk_src_nx_sdk_files ${nx_sdk_src_settings_model_file})
    file(GLOB_RECURSE nx_sdk_documentation_image_files CONFIGURE_DEPENDS
        ${copied_nx_sdk_src_nx_sdk_origin}/doc/*)
    nx_subtract_lists(copied_nx_sdk_src_nx_sdk_files nx_sdk_documentation_image_files)
    list(APPEND depends ${copied_nx_sdk_src_nx_sdk_files})

    # Generate public version of settings_model.md.
    set(copied_nx_sdk_settings_model_origin ${CMAKE_CURRENT_BINARY_DIR})
    set(copied_nx_sdk_settings_model_file
        ${copied_nx_sdk_settings_model_origin}/settings_model.md)
    nx_remove_proprietary_docs(
        ${nx_sdk_src_settings_model_file} ${copied_nx_sdk_settings_model_file}
    )

    _add_analytics_samples()
    _add_cloud_storage_samples()
    _add_storage_samples()
    _add_video_source_samples()
endmacro()

macro(_add_sample_plugin_files target)
    cmake_parse_arguments(PLUGIN "" "" "SOURCE_PATTERNS;EXTRA_FILES" ${ARGN})

    set(descriptor "{\"target\": \"${target}\"}")

    set(origin_dir ${open_source_root}/vms/server/plugins)
    string(JSON descriptor SET ${descriptor} "origin_dir" "\"${origin_dir}\"")

    foreach(source_pattern ${PLUGIN_SOURCE_PATTERNS})
        file(GLOB_RECURSE copied_sample_files
            CONFIGURE_DEPENDS ${origin_dir}/${target}/${source_pattern}
        )
        nx_json_add_array(descriptor APPEND PATH "sources" ARRAY_NAME "copied_sample_files")
        list(APPEND depends ${copied_sample_files})
    endforeach()

    if(PLUGIN_EXTRA_FILES)
        nx_json_add_array(descriptor APPEND PATH "extra_files" ARRAY_NAME "PLUGIN_EXTRA_FILES")
    endif()

    string(JSON new_sample_index LENGTH "${sdk_example_files_json}")
    string(JSON sdk_example_files_json
        SET ${sdk_example_files_json} ${new_sample_index} "${descriptor}"
    )
endmacro()

macro(_add_analytics_samples)
    _add_sample_plugin_files(analytics/sample_analytics_plugin SOURCE_PATTERNS "src/*")
    _add_sample_plugin_files(analytics/stub_analytics_plugin
        SOURCE_PATTERNS "src/*" "object_streamer_files/*"
    )
    _add_sample_plugin_files(analytics/unit_tests/src SOURCE_PATTERNS "*")
endmacro()

macro(_add_cloud_storage_samples)
    _add_sample_plugin_files(cloud_storage/sample_cloud_storage_plugin SOURCE_PATTERNS "src/*")
    _add_sample_plugin_files(cloud_storage/stub_cloud_storage_plugin SOURCE_PATTERNS "src/*")
endmacro()

macro(_add_storage_samples)
    _add_sample_plugin_files(storage/ftp_storage_plugin
        SOURCE_PATTERNS "src/*" "readme.md" EXTRA_FILES ${open_source_root}/artifacts/ftplibpp
    )
    _add_sample_plugin_files(storage/test_storage_plugin SOURCE_PATTERNS "src/*" "readme.md")
endmacro()

macro(_add_video_source_samples)
    _add_sample_plugin_files(device/axis_camera_plugin SOURCE_PATTERNS "src/*" "readme.md")
    _add_sample_plugin_files(device/image_library_plugin SOURCE_PATTERNS "src/*" "readme.md")
    _add_sample_plugin_files(device/rpi_camera_plugin SOURCE_PATTERNS "src/*" "readme.md")
endmacro()

message(STATUS "Generating files for Nx SDK")

set(sdk_name server_plugin_sdk)
set(zip_file ${distribution_output_dir}/${server_plugin_sdk_distribution_name}.zip)
set(sdk_dir ${CMAKE_CURRENT_BINARY_DIR}/${sdk_name})
set(sdk_build_dir "${sdk_dir}-build")
set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(sample_build_log ${logs_dir}/${sdk_name}_sample_build.log)
set(doxygen_log ${logs_dir}/${sdk_name}_doxygen.log)
set(zip_log ${logs_dir}/${sdk_name}_zip.log)

_collect_nx_sdk_file_names()

set(copy_sdk_files_script ${CMAKE_CURRENT_BINARY_DIR}/copy_sdk_files.cmake)
set(sdk_example_files_file ${CMAKE_CURRENT_BINARY_DIR}/copy_sdk_files.json)
list(APPEND depends ${copy_sdk_files_script} ${sdk_example_files_file})

set(msvc_environment_setup_script "call_vcvars64.bat")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(sample_build_script "./build.sh")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(sample_build_script "build.bat")
else()
    message(FATAL_ERROR "Building SDK sample(s) supported only on Windows and Linux.")
endif()
list(APPEND depends
    ${CMAKE_CURRENT_SOURCE_DIR}/files/${sample_build_script}
    ${open_source_root}/build_utils/msvc/${msvc_environment_setup_script}
)

# Substitute variable values into the cmake script which will be called at the build phase.
nx_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/copy_sdk_files.cmake.in
    ${copy_sdk_files_script} @ONLY
)
file(WRITE ${sdk_example_files_file} ${sdk_example_files_json})
nx_store_known_files(${sdk_example_files_file})

# On developer workstations, when not cross-compiling, unit tests for SDK samples will be run
# during the build phase. On CI, such tests are run with other tests, because they are included in
# unit test distribution.

# We can run the SDK tests only if the build platform equals the target platform.
if(targetDevice STREQUAL ${default_target_device})
    # The tests will be run: inform about that.
    set(sample_tests_message " and running unit tests")
else()
    if (targetDevice STREQUAL "windows_x64")
        message(FATAL_ERROR "Cross-compiling for windows_x64 is not supported.")
    endif()
    set(set_env_command ${CMAKE_COMMAND} -E env NX_SDK_NO_TESTS=1)
endif()

if(targetDevice STREQUAL "windows_x64")
    set(compiling_options "")

    if(NOT openSourceBuild)
        # Use MSVC 2019 compiler for non-open-source builds only.
        set(set_env_command ${CMAKE_COMMAND} -E env NX_FORCED_MSVC_VERSION="14.29")
    endif()
else()
    set(compiling_options -DCONAN_SDK-GCC_ROOT=${CONAN_SDK-GCC_ROOT})

    if(targetDevice STREQUAL "linux_arm32" OR targetDevice STREQUAL "edge1")
        set(toolchainFile "linux_arm32")
        if(targetDevice STREQUAL "linux_arm32")
            list(APPEND compiling_options -DrpiFirmware=${CONAN_RPI-FIRMWARE_ROOT})
        endif()
    elseif(targetDevice STREQUAL "linux_arm64")
        set(toolchainFile "linux_arm64")
    elseif(targetDevice STREQUAL "linux_x64")
        set(toolchainFile "linux_x64")
    endif()

    if(toolchainFile)
        list(APPEND compiling_options
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/toolchains/${toolchainFile}.cmake
        )
    endif()
endif()

set(nx_kit_ut_tmp_dir ${CMAKE_CURRENT_BINARY_DIR}/nx_kit_ut_tmp)

list(APPEND compiling_options
    -DQT_DIR="${QT_DIR}"
    -DQT_HOST_PATH="${QT_HOST_PATH}"
    -DNX_KIT_UT_TMP_DIR="${nx_kit_ut_tmp_dir}"
)

# OUTPUT and BYPRODUCTS must be the last arguments - see the function definition.
nx_add_custom_command(
    DEPENDS ${depends}

    COMMAND ${CMAKE_COMMAND} -E remove_directory ${nx_kit_ut_tmp_dir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${nx_kit_ut_tmp_dir}

    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -P ${copy_sdk_files_script} #< Create and populate sdk dir.

    COMMAND ${CMAKE_COMMAND} -E
        echo "  Building samples${sample_tests_message} - see ${sample_build_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir}
        ${set_env_command} ${sample_build_script} ${compiling_options}
        >${sample_build_log} 2>&1

    COMMAND ${CMAKE_COMMAND} -E
        echo "  Running Doxygen - see ${doxygen_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir}
        ${doxygen_executable} >${doxygen_log} 2>&1

    COMMAND ${CMAKE_COMMAND} -E
        echo "  Zipping SDK - see ${zip_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip ${sdk_name} >${zip_log}

    OUTPUT ${zip_file}
    BYPRODUCTS "${sdk_build_dir}" "${sdk_dir}"
)
add_custom_target(nx_server_plugin_sdk ALL DEPENDS ${zip_file})
set_target_properties(nx_server_plugin_sdk PROPERTIES FOLDER sdk)

# Save SDK build directory for usage in other places (e.g. building of the unit test distribution
# archive).
set(nx_server_plugin_sdk_build_dir "${sdk_build_dir}" CACHE INTERNAL "" FORCE)
