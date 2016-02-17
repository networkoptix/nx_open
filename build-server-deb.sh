#!/bin/sh
#mvn package -e --pl=build_environment,build_variables,nx_sdk,nx_storage_sdk,mediaserver,mediaserver_core,common,appserver2,common_libs/nx_email,common_libs/nx_streaming,common_libs/nx_network,common_libs/nx_utils,nx_cloud/cloud_db_client
mvn package -e --pl=build_variables,nx_sdk,nx_storage_sdk,mediaserver,mediaserver_core,common,appserver2,common_libs/nx_email,common_libs/nx_streaming,common_libs/nx_network,common_libs/nx_utils,nx_cloud/cloud_db_client,debsetup/mediaserver-deb
