nx_add_target(nx_aws_client LIBRARY 
    LIBRARY_TYPE SHARED
    NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_network
)

target_compile_definitions(nx_aws_client
    PRIVATE NX_AWS_CLIENT_API=${API_EXPORT_MACRO}
    INTERFACE NX_AWS_CLIENT_API=${API_IMPORT_MACRO})

add_subdirectory(aws_s3_client)
if(withTests)
    add_subdirectory(unit_tests)
endif()
