#!/usr/local/bin/dumb-init /bin/bash

mkdir -p /tmp/core

tmp=$(tempfile)
envsubst < /opt/networkoptix/traffic_relay/etc/traffic_relay.conf > $tmp
mv $tmp /opt/networkoptix/traffic_relay/etc/traffic_relay.conf

if [ -n "$MODULE_CONFIGURATION" ]
then
    config_helper.py /opt/networkoptix/traffic_relay/etc/traffic_relay.conf "$MODULE_CONFIGURATION"
fi

tail --pid $$ -n0 -F /opt/networkoptix/traffic_relay/var/log/log_file.log &

sleep 30

exec /opt/networkoptix/traffic_relay/bin/traffic_relay -e
