nx_add_test(vms_cloud_integration_tests 
    PUBLIC_LIBS
        mediaserver_core libcloud_db GTest GMock)

if(TARGET festival_libs)
    target_link_libraries(vms_cloud_integration_tests PRIVATE festival_libs)
endif()
