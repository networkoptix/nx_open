# Collect file lists to be included into the SDK at the generation phase via "configuring"
# (substituting file list variables into) a dedicated cmake script. Register a build-phase command
# that will zip the SDK directory and test that the SDK samples can be built via build scripts.

include(find_doxygen)

set(depends mediaserver server_plugins) #< Not required technically, but useful to prevent errors.

message(STATUS "Generating files for Camera SDK")

set(sdk_name camera_sdk)
set(zip_file ${distribution_output_dir}/${camera_sdk_distribution_name}.zip)
set(sdk_dir ${CMAKE_CURRENT_BINARY_DIR}/${sdk_name})
set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(sample_build_log ${logs_dir}/${sdk_name}_sample_build.log)
set(doxygen_log ${logs_dir}/${sdk_name}_doxygen.log)
set(zip_log ${logs_dir}/${sdk_name}_zip.log)

# Collect source files for statically prepared files.
set(copied_static_origin ${CMAKE_CURRENT_SOURCE_DIR}/files)
file(GLOB_RECURSE copied_static_files ${copied_static_origin}/*)

# Collect source files for nx_kit.
set(copied_nx_kit_origin ${CMAKE_SOURCE_DIR}/open/artifacts/nx_kit)
file(GLOB_RECURSE copied_nx_kit_files ${copied_nx_kit_origin}/*)

# Collect source files for SDK.
set(copied_nx_sdk_src_origin ${CMAKE_SOURCE_DIR}/vms/libs/nx_sdk/src)
set(copied_nx_sdk_src_files
    ${copied_nx_sdk_src_origin}/plugins/plugin_api.h
    ${copied_nx_sdk_src_origin}/plugins/plugin_tools.h
    ${copied_nx_sdk_src_origin}/camera/camera_plugin.h
    ${copied_nx_sdk_src_origin}/camera/camera_plugin_types.h
    ${copied_nx_sdk_src_origin}/nx/sdk/ptr.h
)

# Collect source files for the sample - Axis Camera Plugin.
set(copied_sample_dir ${CMAKE_SOURCE_DIR}/vms/server/plugins/device)
set(copied_sample_1_target axis_camera_plugin/src)
set(copied_sample_1_origin ${copied_sample_dir}/${copied_sample_1_target})
file(GLOB_RECURSE copied_sample_1_files ${copied_sample_1_origin}/*)
set(copied_sample_2_target image_library_plugin/src)
set(copied_sample_2_origin ${copied_sample_dir}/${copied_sample_1_target})
file(GLOB_RECURSE copied_sample_2_files ${copied_sample_1_origin}/*)

set(copy_sdk_files_script ${CMAKE_CURRENT_BINARY_DIR}/copy_sdk_files.cmake)

# Substitute variable values into the cmake script which will be called at the build phase.
nx_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/copy_sdk_files.cmake.in
    ${copy_sdk_files_script} @ONLY)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(sample_build_script "./build_samples.sh")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(sample_build_script "build_samples.bat")
else()
    message(FATAL_ERROR "Building SDK sample(s) supported only on Windows and Linux.")
endif()

if(targetDevice STREQUAL "windows-x64")
    set(compilingOptions "")
else()
    if(targetDevice STREQUAL "linux_arm32"
        OR targetDevice STREQUAL "bpi"
        OR targetDevice STREQUAL "edge1"
    )
        set(toolchainFile "linux_arm32")
    elseif(targetDevice STREQUAL "linux_arm64")
        set(toolchainFile "linux_arm64")
    elseif(targetDevice STREQUAL "linux-x64")
        set(toolchainFile "linux_x64")
    endif()

    set(compilingOptions -DPACKAGES_DIR=${PACKAGES_DIR})

    if(toolchainFile)
        set(compilingOptions ${compilingOptions}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/toolchains/${toolchainFile}.cmake)
    endif()
endif()

# Command to be executed at the build phase.
add_custom_command(OUTPUT ${zip_file}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${sdk_dir} #< Delete old sdk dir (if any).
    COMMAND ${CMAKE_COMMAND} -P ${copy_sdk_files_script} #< Create and populate sdk dir.

    COMMAND ${CMAKE_COMMAND} -E echo
        "  Building samples - see ${sample_build_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir}
        ${sample_build_script} ${compilingOptions} -DCMAKE_PREFIX_PATH=${QT_DIR}
        >${sample_build_log} 2>&1
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${sdk_dir}-build #< Delete sdk build dir.

    COMMAND ${CMAKE_COMMAND} -E echo
        "  Running Doxygen - see ${doxygen_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir} ${doxygen_executable} >${doxygen_log} 2>&1

    COMMAND ${CMAKE_COMMAND} -E echo
        "  Zipping SDK - see ${zip_log}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} #< Needed for zipping.
    COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip ${sdk_name} >${zip_log}

    COMMAND ${CMAKE_COMMAND} -E remove_directory ${sdk_dir} #< Delete sdk dir.

    DEPENDS
        ${copy_sdk_files_script}
        ${CMAKE_CURRENT_SOURCE_DIR}/files/${sample_build_script}
        ${copied_static_files}
        ${copied_nx_kit_files}
        ${copied_nx_sdk_src_files}
        ${copied_sample_files}
)

add_custom_target(nx_camera_sdk ALL DEPENDS ${zip_file})
