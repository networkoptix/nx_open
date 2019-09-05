#!/usr/local/bin/dumb-init /bin/bash

mkdir -p /tmp/core /opt/networkoptix/storage_service/etc

if [ -n "$MODULE_CONFIGURATION" ]
then
	    config_helper /opt/networkoptix/storage_service/etc/storage_service.conf "$MODULE_CONFIGURATION"
fi

exec /opt/networkoptix/storage_service/bin/storage_service -e
