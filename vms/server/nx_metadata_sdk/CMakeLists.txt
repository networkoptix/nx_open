# Collect file lists to be included into the SDK at the generation phase via "configuring"
# (substituting file list variables into) a dedicated cmake script. Register a build-phase command
# that will zip the SDK directory and test that the SDK samples can be built via build scripts.

include(find_doxygen)

set(depends mediaserver server_plugins) #< Not required technically, but useful to prevent errors.

message(STATUS "Generating files for Metadata SDK")

set(sdk_name metadata_sdk)
set(zip_file ${distribution_output_dir}/${metadata_sdk_distribution_name}.zip)
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
set(copied_nx_sdk_src_nx_origin ${CMAKE_SOURCE_DIR}/vms/libs/nx_sdk/src/nx)
file(GLOB_RECURSE copied_nx_sdk_src_nx_files ${copied_nx_sdk_src_nx_origin}/*)
set(copied_nx_sdk_unit_tests_origin ${CMAKE_SOURCE_DIR}/vms/libs/nx_sdk/unit_tests)
file(GLOB_RECURSE copied_nx_sdk_unit_tests_files ${copied_nx_sdk_unit_tests_origin}/*)


set(copied_samples_dir ${CMAKE_SOURCE_DIR}/vms/server/plugins/analytics)

# Collect source files for the sample 1 - Sample Analytics Plugin.
set(copied_sample_1_target sample_analytics_plugin)
set(copied_sample_1_origin ${copied_samples_dir}/${copied_sample_1_target})
file(GLOB_RECURSE copied_sample_1_files ${copied_sample_1_origin}/src/*)

# Collect source files for the sample 2 - Stub Analytics Plugin.
set(copied_sample_2_target stub_analytics_plugin)
set(copied_sample_2_origin ${copied_samples_dir}/${copied_sample_2_target})
file(GLOB_RECURSE copied_sample_2_files ${copied_sample_2_origin}/src/*)
set(copied_sample_unit_tests_target stub_analytics_plugin/unit_tests/src)
set(copied_sample_unit_tests_origin ${copied_samples_dir}/${copied_sample_unit_tests_target})
file(GLOB_RECURSE copied_sample_unit_tests_files ${copied_sample_unit_tests_origin}/*)

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
add_custom_command(OUTPUT ${zip_file}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${sdk_dir} #< Delete old sdk dir (if any).
    COMMAND ${CMAKE_COMMAND} -P ${copy_sdk_files_script} #< Create and populate sdk dir.

    COMMAND ${CMAKE_COMMAND} -E echo
        "  Building samples${sample_tests_message} - see ${sample_build_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${sdk_dir}
        ${sample_build_script} ${sampleTestsTrigger} ${compilingOptions}
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
        ${copied_nx_sdk_src_nx_files}
        ${copied_sample_files}
        ${copied_sample_unit_tests_files}
)

add_custom_target(nx_metadata_sdk ALL DEPENDS ${zip_file})
set_target_properties(nx_metadata_sdk PROPERTIES FOLDER sdk)
