function(nx_sign_windows_executable target)
    find_program(SIGNTOOL_EXECUTABLE signtool.exe ${signtool_directory})
    if(NOT SIGNTOOL_EXECUTABLE)
        message(FATAL_ERROR "Cannot find signtool.exe")
    endif()

    nx_find_first_matching_file(certificate_file "${certificates_path}/wixsetup/*.p12")
    if(NOT certificate_file)
        message(FATAL_ERROR "Cannot find certificate file in ${certificates_path}/wixsetup/")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${SIGNTOOL_EXECUTABLE} sign
            /td sha256
            /fd sha256
            /tr "http://tsa.startssl.com/rfc3161"
            /v
            /a
            /f ${certificate_file}
            /p ${sign.password}
            /d "${company.name} ${display.product.name}"
            $<TARGET_FILE:${target}>
    )
endfunction()
