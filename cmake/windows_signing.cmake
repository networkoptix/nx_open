nx_option(hardwareSigning
    "Sign windows binaries with hardware smart card key"
    OFF)

cmake_dependent_option(trustedTimestamping
    "Sign windows binaries with trusted timestamp"
    ON "NOT developerBuild"
    OFF
)

set(timestamp_server_parameters "")
if(trustedTimestamping)
    set(signing_timestamp_server "http://timestamp.comodoca.com/rfc3161")
    set(timestamp_server_parameters /td sha256 /tr ${signing_timestamp_server})
endif()

function(nx_get_windows_sign_command variable)
    find_program(SIGNTOOL_EXECUTABLE signtool.exe
        PATHS ${signtool_directory}/bin
        NO_DEFAULT_PATH
    )
    if(NOT SIGNTOOL_EXECUTABLE)
        message(FATAL_ERROR "Cannot find signtool.exe")
    endif()

    set(signing_parameters
        ${SIGNTOOL_EXECUTABLE} sign
        /fd sha256
        ${timestamp_server_parameters}
        /v)

    if(hardwareSigning)
        set(signing_parameters ${signing_parameters} /a)
    else()
        nx_find_first_matching_file(certificate_file "${certificates_path}/wixsetup/*.p12")
        if(NOT certificate_file)
            message(FATAL_ERROR "Cannot find certificate file in ${certificates_path}/wixsetup/")
        endif()

        set(signing_parameters ${signing_parameters}
            /f ${certificate_file}
            /p ${sign.password}
            /d "${company.name} ${display.product.name}")
    endif()
    set(${variable} ${signing_parameters} PARENT_SCOPE)
endfunction()

function(nx_sign_windows_executable target)
    set(signCommand "")
    nx_get_windows_sign_command(signCommand)

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${signCommand}  $<TARGET_FILE:${target}>
    )
endfunction()
