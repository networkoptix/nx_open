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

function(nx_get_windows_sign_command variable)
    set(signing_parameters
        ${PYTHON_EXECUTABLE} ${windows_sign_tool}
        ${trusted_timestamping_parameters}
        ${config_file_parameters}
        ${additional_windows_sign_tool_parameters}
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
