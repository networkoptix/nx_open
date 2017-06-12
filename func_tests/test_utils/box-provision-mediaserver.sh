#!/bin/bash -xe

MEDIASERVER_CONF=/opt/$COMPANY_NAME/mediaserver/etc/mediaserver.conf
MEDIASERVER_CONF_INITIAL=$MEDIASERVER_CONF.initial
export DEBIAN_FRONTEND=noninteractive

# server may crash several times during one test, but we need to keep all cores; %t=timestamp, %p=pid
echo "kernel.core_pattern=core.%t.%p" > /etc/sysctl.d/60-core-pattern.conf
service procps start

echo "Install mediaserver dependencies..."
apt-get update
apt-get -y install \
        libaudio2 \
        libcrypt-passwdmd5-perl \
        mtools \
        syslinux \
        syslinux-common \
        nas \
        floppyd \
        cifs-utils

echo "Install mediaserver..."
dpkg -i --force-depends /vagrant/mediaserver.deb

while ! nc -z localhost 7001; do
	echo "Server is not started yet; waiting..."
	sleep 1
done

cp $MEDIASERVER_CONF $MEDIASERVER_CONF_INITIAL
