nx_add_target(appserver2 LIBRARY PUBLIC_LIBS common cloud_db_client nx_fusion nx_network)

if(withTests AND (withMediaServer OR withClouds))
    add_subdirectory(unit_tests)
endif()
