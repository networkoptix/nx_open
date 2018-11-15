if(withClouds OR withDesktopClient)
    add_subdirectory(librelaying)
endif()

if(withClouds)
    add_subdirectory(nx_data_sync_engine)
    add_subdirectory(nx_key_value_db)
endif()
