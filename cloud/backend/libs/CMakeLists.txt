if(withClouds OR withDesktopClient)
    add_subdirectory(librelaying)
endif()

if(withClouds)
    add_subdirectory(nx_clusterdb_engine)
    add_subdirectory(nx_clusterdb_map)
endif()
