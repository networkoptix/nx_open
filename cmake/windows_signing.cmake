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

function(nx_sign_windows_executable target)
    find_program(SIGNTOOL_EXECUTABLE signtool.exe
        PATHS ${signtool_directory}/bin
        NO_DEFAULT_PATH
    )
    if(NOT SIGNTOOL_EXECUTABLE)
        message(FATAL_ERROR "Cannot find signtool.exe")
    endif()

    if(hardwareSigning)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${SIGNTOOL_EXECUTABLE} sign
                /fd sha256
                ${timestamp_server_parameters}
                /v
                /a
                $<TARGET_FILE:${target}>
        )
    else()
        nx_find_first_matching_file(certificate_file "${certificates_path}/wixsetup/*.p12")
        if(NOT certificate_file)
            message(FATAL_ERROR "Cannot find certificate file in ${certificates_path}/wixsetup/")
        endif()

        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${SIGNTOOL_EXECUTABLE} sign
                /fd sha256
                ${timestamp_server_parameters}
                /v
                /f ${certificate_file}
                /p ${sign.password}
                /d "${company.name} ${display.product.name}"
                $<TARGET_FILE:${target}>
        )
    endif()


endfunction()
