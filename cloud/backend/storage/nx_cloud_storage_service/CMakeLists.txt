nx_add_target(nx_cloud_storage_service LIBRARY NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        cloud_db_client nx_clusterdb_engine nx_sql nx_geo_ip nx_cloud_storage_client
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
