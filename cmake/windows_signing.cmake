## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

if(NOT WINDOWS)
    return()
endif()

set(trusted_timestamping_parameters "")

# Open-source signtool.py does not use " --trusted-timestamping" parameter - it uses the config
# file to decide if it should use trusted timestamping feature.
if(NOT openSourceBuild AND trustedTimestamping)
    set(trusted_timestamping_parameters --trusted-timestamping)
endif()

if(signtoolConfig)
    set(config_file_parameters --config "${signtoolConfig}")
endif()

set(windows_sign_tool "${open_build_utils_dir}/signtool/signtool.py")

# Sign tool option for the package type (statistics). Open-source signtool.py lacks it.
set(windows_sign_tool_package_type_option "")

# Sign tool parameters for the libraries packed into the installers, overriding the ones above.
# The proprietary build signs them with the Nx certificate instead of the customization one.
set(windows_library_sign_tool_parameters "")

# Sign tool parameters for signing the installer payload, e.g. to bound the sign tool's own retries:
# sign_many.py retries as well.
set(windows_payload_sign_tool_parameters "")

# Identify the certificates of the installer executables and libraries. A payload file signed with
# another one, e.g. by a build of another customization in the same build directory, is re-signed.
set(windows_signer_id default)
set(windows_library_signer_id default)

# Records the payload files signed by nx_get_wix_payload_sign_commands(). Kept over the pre-build
# clean of ninja_tool by builds without signing too: without it, a later signing build keeps the
# files validly signed with another certificate, e.g. of another customization.
set(windows_payload_sign_record_file ${CMAKE_BINARY_DIR}/sign_many_record.json)
nx_store_known_file(${windows_payload_sign_record_file})

# PACKAGE_TYPE is passed to the sign tool, if it supports the option. EXTRA_PARAMETERS go after all
# the others, so they override them.
function(nx_get_windows_sign_command variable)
    cmake_parse_arguments(ARG "" "PACKAGE_TYPE" "EXTRA_PARAMETERS" ${ARGN})

    set(package_type_parameters "")
    if(ARG_PACKAGE_TYPE AND windows_sign_tool_package_type_option)
        set(package_type_parameters ${windows_sign_tool_package_type_option} ${ARG_PACKAGE_TYPE})
    endif()

    set(signing_parameters
        ${PYTHON_EXECUTABLE} ${windows_sign_tool}
        ${trusted_timestamping_parameters}
        ${config_file_parameters}
        ${additional_windows_sign_tool_parameters}
        ${package_type_parameters}
        ${ARG_EXTRA_PARAMETERS}
        --file)
    set(${variable} ${signing_parameters} PARENT_SCOPE)
endfunction()

function(nx_sign_windows_executable target)
    set(signCommand "")
    nx_get_windows_sign_command(signCommand)

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${signCommand} $<TARGET_FILE:${target}>
            --output $<TARGET_FILE:${target}>
    )
endfunction()

# Get the commands signing the executables and libraries which light packs from WIXOBJ_FILES,
# skipping validly signed ones (e.g. Microsoft redistributables), as a list starting with COMMAND.
# They must run right before light: the payload is built by then, and nothing else in the build
# writes it. They must not run concurrently with another call's commands: the payloads overlap, and
# they share the record file. Executables are signed like nx_sign_windows_executable() does,
# libraries with windows_library_sign_tool_parameters. PACKAGE_TYPE is passed to
# nx_get_windows_sign_command().
function(nx_get_wix_payload_sign_commands variable)
    cmake_parse_arguments(ARG "" "PACKAGE_TYPE" "WIXOBJ_FILES" ${ARGN})

    set(exe_sign_command "")
    nx_get_windows_sign_command(exe_sign_command
        PACKAGE_TYPE ${ARG_PACKAGE_TYPE}
        EXTRA_PARAMETERS ${windows_payload_sign_tool_parameters})
    set(library_sign_command "")
    nx_get_windows_sign_command(library_sign_command
        PACKAGE_TYPE ${ARG_PACKAGE_TYPE}
        EXTRA_PARAMETERS
            ${windows_payload_sign_tool_parameters}
            ${windows_library_sign_tool_parameters})

    set(sign_many_command ${PYTHON_EXECUTABLE} ${open_build_utils_dir}/signtool/sign_many.py)
    foreach(wixobj ${ARG_WIXOBJ_FILES})
        list(APPEND sign_many_command --wixobj ${wixobj})
    endforeach()
    list(APPEND sign_many_command
        --skip-signed
        --record-file ${windows_payload_sign_record_file}
        --temp-dir ${CMAKE_BINARY_DIR}/sign_many_tmp)
    # A release must not ship a file signed without a timestamp, e.g. by an earlier private build.
    if(trustedTimestamping)
        list(APPEND sign_many_command --require-timestamp)
    endif()

    set(${variable}
        COMMAND ${sign_many_command} --pattern *.exe
            --signer-id ${windows_signer_id}
            -- ${exe_sign_command}
        COMMAND ${sign_many_command} --pattern *.dll --pattern *.pyd
            --signer-id ${windows_library_signer_id}
            -- ${library_sign_command}
        PARENT_SCOPE)
endfunction()
