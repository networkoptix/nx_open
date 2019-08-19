#!/bin/bash

BASEDIR=/opt/networkoptix/cloud_connect_test_util
CLOUD_HOST=$1; shift

[ -n "$CLOUD_HOST" ] && patch-cloud-host.sh "$CLOUD_HOST"

exec $BASEDIR/bin/cloud_connect_test_util $@
