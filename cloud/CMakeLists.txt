## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(NOT cloudHost)
    message(FATAL_ERROR "Cloud Host is not specified; use -DcustomCloudHost= or -Dcustomization=")
endif()


add_subdirectory(cloud_db_client)
add_subdirectory(channel_partner_service)
