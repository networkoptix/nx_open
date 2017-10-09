#!/usr/local/bin/dumb-init /bin/bash

mkdir -p /tmp/core

tmp=$(tempfile)
envsubst < /opt/networkoptix/connection_mediator/etc/connection_mediator.conf > $tmp
mv $tmp /opt/networkoptix/connection_mediator/etc/connection_mediator.conf

if [ -n "$MODULE_CONFIGURATION" ]
then
    config_helper.py /opt/networkoptix/connection_mediator/etc/connection_mediator.conf "$MODULE_CONFIGURATION"
fi

tail --pid $$ -n0 -F /opt/networkoptix/connection_mediator/var/log/log_file.log &

exec /opt/networkoptix/connection_mediator/bin/connection_mediator -e
