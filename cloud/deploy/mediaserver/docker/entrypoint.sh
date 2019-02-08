#!/usr/local/bin/dumb-init /bin/bash

BASEDIR=opt/networkoptix/mediaserver

[ -d $BASEDIR/etc ] || { echo $BASEDIR/etc should be mounted; exit 1; }
[ -d $BASEDIR/var ] || { echo $BASEDIR/var should be mounted; exit 1; }

mkdir -p $BASEDIR/var/log
tail --pid $$ -n0 -F $BASEDIR/var/log/log_file.log &

exec $BASEDIR/bin/mediaserver -e
