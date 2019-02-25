set(nx_enable_werror ON)

add_subdirectory(libs)
add_subdirectory(cloud_db_client)
add_subdirectory(nx_cloud_connectivity)

if(withClouds OR withDesktopClient)
    add_subdirectory(libvms_gateway_core)
endif()

if(withClouds)
    add_subdirectory(vms_gateway)
    add_subdirectory(libconnection_mediator)
    add_subdirectory(connection_mediator)
    add_subdirectory(discovery)
    add_subdirectory(libcloud_db)
    add_subdirectory(cloud_db)
    add_subdirectory(libtraffic_relay)
    add_subdirectory(traffic_relay)
    add_subdirectory(time_server)
    add_subdirectory(cloud_connect_test_util)

    if(withTests)
        add_subdirectory(tests)
    endif()
endif()

add_custom_target(cloud-backend DEPENDS cloud_db_client vms_gateway connection_mediator cloud_db traffic_relay time_server cloud_connect_test_util)
