nx_add_target(nx_cloud_storage_client LIBRARY 
    LIBRARY_TYPE SHARED
    NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_network nx_aws_client
)

target_compile_definitions(nx_cloud_storage_client
    PRIVATE NX_CLOUD_STORAGE_CLIENT_API=${API_EXPORT_MACRO}
    INTERFACE NX_CLOUD_STORAGE_CLIENT_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
