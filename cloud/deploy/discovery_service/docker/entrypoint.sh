#!/usr/local/bin/dumb-init /bin/bash

mkdir -p /opt/networkoptix/discovery_service/bin/

DISCOVERY_SERVICE_HTTP_PORT=${DISCOVERY_SERVICE_HTTP_PORT:-3367}

exec /opt/networkoptix/discovery_service/bin/discovery_service --http-port=$DISCOVERY_SERVICE_HTTP_PORT
