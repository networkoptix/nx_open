#!/usr/local/bin/dumb-init /bin/bash

mkdir -p /tmp/core

export INSTANCE_IP=$CONNECTION_MEDIATOR_PUBLIC_IP
[ -n "$INSTANCE_IP" ] || INSTANCE_IP=$(LD_LIBRARY_PATH= wget -q -O- networkoptix.com/myip)

tmp=$(tempfile)
envsubst < /opt/networkoptix/cloud_db/etc/cloud_db.conf > $tmp
mv $tmp /opt/networkoptix/cloud_db/etc/cloud_db.conf

tmp=$(tempfile)
envsubst < /opt/networkoptix/cloud_db/etc/cloud_modules_template.xml > $tmp
mv $tmp /opt/networkoptix/cloud_db/etc/cloud_modules_template.xml

tmp=$(tempfile)
envsubst < /opt/networkoptix/cloud_db/etc/cloud_modules_new_template.xml > $tmp
mv $tmp /opt/networkoptix/cloud_db/etc/cloud_modules_new_template.xml

if [ -n "$MODULE_CONFIGURATION" ]
then
    config_helper.py /opt/networkoptix/cloud_db/etc/cloud_db.conf "$MODULE_CONFIGURATION"
fi

tail --pid $$ -n0 -F /opt/networkoptix/cloud_db/var/log/log_file.log | egrep -v 'DEBUG|VERBOSE' &

exec /opt/networkoptix/cloud_db/bin/cloud_db -e
