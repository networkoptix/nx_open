#!/bin/bash -xe

MEDIASERVER_CONF=/opt/$COMPANY_NAME/mediaserver/etc/mediaserver.conf
MEDIASERVER_CONF_INITIAL=$MEDIASERVER_CONF.initial
export DEBIAN_FRONTEND=noninteractive

echo "Install mediaserver dependencies..."
apt-get update
apt-get -y install \
        libaudio2 \
        libcrypt-passwdmd5-perl \
        mtools \
        syslinux \
        syslinux-common \
        nas \
        floppyd

echo "Install mediaserver..."
dpkg -i --force-depends /vagrant/mediaserver.deb

while ! nc -z localhost 7001; do
	echo "Server is not started yet; waiting..."
	sleep 1
done

cp $MEDIASERVER_CONF $MEDIASERVER_CONF_INITIAL
