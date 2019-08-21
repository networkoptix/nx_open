# Collect file lists to be included into the SDK at the generation phase via "configuring"
# (substituting file list variables into) a dedicated cmake script. Register a build-phase command
# that will zip the SDK directory and test that the SDK samples can be built via build scripts.

include(find_doxygen)

set(depends mediaserver server_plugins) #< Not required technically, but useful to prevent errors.

message(STATUS "Generating files for Storage SDK")

set(sdk_name storage_sdk)
set(zip_file ${distribution_output_dir}/${storage_sdk_distribution_name}.zip)
set(server_resource_zip_file ${CMAKE_CURRENT_BINARY_DIR}/../sdk.zip)
set(sdk_dir ${CMAKE_CURRENT_BINARY_DIR}/${sdk_name})
set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(sample_build_log ${logs_dir}/${sdk_name}_sample_build.log)
set(doxygen_log ${logs_dir}/${sdk_name}_doxygen.log)
set(zip_log ${logs_dir}/${sdk_name}_zip.log)

# Collect source files for statically prepared files.
set(copied_static_origin ${CMAKE_CURRENT_SOURCE_DIR}/files)
file(GLOB_RECURSE copied_static_files ${copied_static_origin}/*)

# Collect source files for SDK.
set(copied_nx_sdk_src_origin ${CMAKE_SOURCE_DIR}/vms/libs/nx_sdk/src)
set(copied_nx_sdk_src_files
    ${copied_nx_sdk_src_origin}/plugins/plugin_api.h
    ${copied_nx_sdk_src_origin}/storage/third_party_storage.h
)

set(copied_samples_dir ${CMAKE_SOURCE_DIR}/vms/server/plugins/storage)

# Collect source files for the sample 1 - FTP Storage Plugin.
set(copied_sample_1_target ftp_storage_plugin)
set(copied_sample_1_origin ${copied_samples_dir}/${copied_sample_1_target})
file(GLOB_RECURSE copied_sample_1_files ${copied_sample_1_origin}/src/*)
list(APPEND copied_sample_1_files ${copied_sample_1_origin}/readme.md)

# Collect source files for the sample 2 - Test Storage Plugin.
set(copied_sample_2_target test_storage_plugin)
set(copied_sample_2_origin ${copied_samples_dir}/${copied_sample_2_target})
file(GLOB_RECURSE copied_sample_2_files ${copied_sample_2_origin}/src/*)
list(APPEND copied_sample_2_files ${copied_sample_2_origin}/readme.md)

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
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/toolchains/${toolchainFile}.cmake
        )
    endif()
endif()

# Command to be executed at the build phase.
add_custom_command(OUTPUT ${zip_file} ${server_resource_zip_file}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${sdk_dir} #< Delete old sdk dir (if any).
    COMMAND ${CMAKE_COMMAND} -P ${copy_sdk_files_script} #< Create and populate sdk dir.

    COMMAND ${CMAKE_COMMAND} -E echo
        "  Building samples - see ${sample_build_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir}
        ${sample_build_script} ${compilingOptions}
        >${sample_build_log} 2>&1
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${sdk_dir}-build #< Delete sdk build dir.

    COMMAND ${CMAKE_COMMAND} -E echo
        "  Running Doxygen - see ${doxygen_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir} ${doxygen_executable} >${doxygen_log} 2>&1

    COMMAND ${CMAKE_COMMAND} -E echo
        "  Zipping SDK - see ${zip_log}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} #< Needed for zipping.
    COMMAND ${CMAKE_COMMAND} -E tar cvf ${zip_file} --format=zip ${sdk_name} >${zip_log}

    COMMAND ${CMAKE_COMMAND} -E copy ${zip_file} ${server_resource_zip_file}

    COMMAND ${CMAKE_COMMAND} -E remove_directory ${sdk_dir} #< Delete sdk dir.

    DEPENDS
        ${copy_sdk_files_script}
        ${CMAKE_CURRENT_SOURCE_DIR}/files/${sample_build_script}
        ${copied_static_files}
        ${copied_nx_sdk_src_files}
        ${copied_sample_files}
)

add_custom_target(nx_storage_sdk ALL DEPENDS ${zip_file})
