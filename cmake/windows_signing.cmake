include_guard(GLOBAL)

if(NOT customization.id)
    # Customization may forecefully disable the code signing.
    message(FATAL_ERROR "Code signing must be initialized after customization is loaded")
endif()

if(NOT WINDOWS)
    return()
endif()

include(code_signing)

# Enable trusted timestamping for all publication types intended for end users. Do not sign
# local developer builds as well as private QA builds.
set(trustedTimestamping true)
set(_disableTrustedTimestampingPublicationTypes "local" "private")
if(publicationType IN_LIST _disableTrustedTimestampingPublicationTypes)
    set(trustedTimestamping false)
endif()
unset(_disableTrustedTimestampingPublicationTypes)
message(STATUS
    "Trusted timestaping is ${trustedTimestamping} for the ${publicationType} publication type")

set(trusted_timestamping_parameters "")
if(trustedTimestamping)
    set(trusted_timestamping_parameters --trusted-timestamping)
endif()

function(nx_get_windows_sign_command variable)
    set(signing_parameters
        ${PYTHON_EXECUTABLE} ${build_utils_dir}/sign_binary.py
        --url ${signingServer}
        --customization ${customization}
        ${trusted_timestamping_parameters}
        --file)
    set(${variable} ${signing_parameters} PARENT_SCOPE)
endfunction()

function(nx_sign_windows_executable target)
    set(signCommand "")
    nx_get_windows_sign_command(signCommand)

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${signCommand}  $<TARGET_FILE:${target}>
    )
endfunction()
