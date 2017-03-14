#!/bin/bash -xe

MEDIASERVER_CONF=/opt/$COMPANY_NAME/mediaserver/etc/mediaserver.conf
MEDIASERVER_CONF_INITIAL=$MEDIASERVER_CONF.initial
export DEBIAN_FRONTEND=noninteractive

dpkg -i --force-depends /vagrant/mediaserver.deb

while ! nc -z localhost 7001; do
	echo "Server is not started yet; waiting..."
	sleep 1
done

cp $MEDIASERVER_CONF $MEDIASERVER_CONF_INITIAL
