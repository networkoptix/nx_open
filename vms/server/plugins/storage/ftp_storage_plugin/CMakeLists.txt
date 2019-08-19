nx_add_server_plugin(ftp_storage_plugin NO_MOC NO_GTEST OPTIONAL
    PRIVATE_LIBS
        nx_sdk
    ADD_TO_DISTRIBUTION_IF FALSE
)

target_compile_definitions(ftp_storage_plugin PRIVATE
    # Needed for ftplib.
    -DNOSSL
    -DNOLFS
)

if(LINUX)
    target_compile_options(ftp_storage_plugin PRIVATE
        # Suppress incorrect gcc warnings in ftplib.
        -Wno-stringop-truncation
    )
endif()
