#!/usr/local/bin/dumb-init /bin/bash

mkdir -p /tmp/core

tmp=$(tempfile)
envsubst < /opt/networkoptix/cloud_db/etc/cloud_db.conf > $tmp
mv $tmp /opt/networkoptix/cloud_db/etc/cloud_db.conf

tail --pid $$ -n0 -F /opt/networkoptix/cloud_db/var/log/log_file.log &

exec /opt/networkoptix/cloud_db/bin/cloud_db -e
