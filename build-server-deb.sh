#!/bin/sh
if [ -n "$1" ]; then
    threads="-T $1"
else
    threads=""
fi
projects=build_variables,nx_sdk,nx_storage_sdk,common,appserver2,common,common_libs/nxemail,mediaserver_core,mediaserver,debsetup/mediaserver-deb
mvn package -e $threads --pl=$projects

