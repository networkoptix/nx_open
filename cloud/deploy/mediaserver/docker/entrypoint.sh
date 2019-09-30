#!/bin/bash

BASEDIR=/opt/networkoptix/mediaserver
CLOUD_HOST=$1; shift

mkdir -p $BASEDIR/var/log
tail --pid $$ -n0 -F $BASEDIR/var/log/log_file.log &

[ -n "$CLOUD_HOST" ] && patch-cloud-host.sh "$CLOUD_HOST"

exec $BASEDIR/bin/mediaserver -e
