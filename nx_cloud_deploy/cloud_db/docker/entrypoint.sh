#!/usr/local/bin/dumb-init /bin/bash

mkdir -p /tmp/core

MEDIATOR_STUN_PORT=${MEDIATOR_STUN_PORT:-3345}
MEDIATOR_HTTP_PORT=${MEDIATOR_HTTP_PORT:-3355}

[ -n "$MEDIATOR_HOST" ] || MEDIATOR_HOST="$CONNECTION_MEDIATOR_PUBLIC_IP"
[ -n "$MEDIATOR_HOST" ] || MEDIATOR_HOST=$(LD_LIBRARY_PATH= wget -q -O- networkoptix.com/myip)

export MEDIATOR_STUN_PORT MEDIATOR_HTTP_PORT MEDIATOR_HOST

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

exec /opt/networkoptix/cloud_db/bin/cloud_db -e
