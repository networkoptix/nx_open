#!/usr/local/bin/dumb-init /bin/bash

mkdir -p /opt/networkoptix/mediaserver/{etc,var/log,var/ssl}
mkdir -p /root/.config/nx_ini
tail --pid $$ -n0 -F /opt/networkoptix/mediaserver/var/log/log_file.log &

exec /opt/networkoptix/mediaserver/bin/mediaserver -e