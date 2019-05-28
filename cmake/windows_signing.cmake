include(CMakeDependentOption)

set(signingServer "http://localhost:8080" CACHE STRING "Signing server address")

cmake_dependent_option(trustedTimestamping
    "Sign windows binaries with trusted timestamp"
    ON "NOT developerBuild"
    OFF
)

if(NOT build_utils_dir)
    set(build_utils_dir "${CMAKE_SOURCE_DIR}/build_utils")
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
