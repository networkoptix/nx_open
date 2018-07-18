nx_add_target(cloud_db_client LIBRARY NO_MOC 
    PUBLIC_LIBS nx_network
)

target_include_directories(cloud_db_client PUBLIC src/include)
