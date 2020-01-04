#!/usr/local/bin/dumb-init /bin/bash

mkdir -p /opt/networkoptix/discovery_service/bin/
mkdir -p /opt/networkoptix/discovery_service/var/log/

DISCOVERY_SERVICE_HTTP_PORT=${DISCOVERY_SERVICE_HTTP_PORT:-3367}
DISCOVERY_SERVICE_LOG_FILE=${DISCOVERY_SERVICE_LOG_FILE:-/opt/networkoptix/discovery_service/var/log/discovery_service.log}

export AWS_REGION=${AWS_REGION}
export DISCOVERY_TABLE_NAME=${DISCOVERY_TABLE_NAME}

exec /opt/networkoptix/discovery_service/bin/discovery_service --http-port=$DISCOVERY_SERVICE_HTTP_PORT --log-file=$DISCOVERY_SERVICE_LOG_FILE
