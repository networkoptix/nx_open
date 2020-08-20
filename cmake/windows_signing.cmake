include_guard(GLOBAL)

if(NOT customization.id)
    # Customization may forecefully disable the code signing.
    message(FATAL_ERROR "Code signing must be initialized after customization is loaded")
endif()

if(NOT WINDOWS OR NOT codeSigning)
    return()
endif()

set(signingServer "http://localhost:8080" CACHE STRING "Signing server address")

# Enable trusted timestamping for all publication types intended for end users. Do not sign
# local developer builds as well as private QA builds.
set(trustedTimestamping ON)
set(_disableTrustedTimestampingPublicationTypes "local" "private")
if(publicationType IN_LIST _disableTrustedTimestampingPublicationTypes)
    set(trustedTimestamping OFF)
endif()
unset(_disableTrustedTimestampingPublicationTypes)
message(STATUS
    "Trusted timestaping is ${trustedTimestamping} for the ${publicationType} publication type")

if(NOT build_utils_dir)
    set(build_utils_dir "${CMAKE_SOURCE_DIR}/build_utils")
    message(STATUS "Build utils directory ${build_utils_dir}")
endif()

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
