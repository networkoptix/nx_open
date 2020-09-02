include_guard(GLOBAL)

include(code_signing)

function(nx_create_openssl_signature target file)
    set(signatureFile "${file}.sig")
    set(signCommand
        ${PYTHON_EXECUTABLE} ${build_utils_dir}/code_signing/openssl_client.py
        --url ${signingServer}
        --file ${file}
        --output ${signatureFile})

    if(codeSigning)
        add_custom_command(
            COMMENT "Generating signature for the ${file}"
            TARGET ${target}
            POST_BUILD
            COMMAND ${signCommand})
    endif()
endfunction()
