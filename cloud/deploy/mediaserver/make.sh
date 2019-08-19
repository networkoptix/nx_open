#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=mediaserver
CLOUD_HOST=stage.nxvms.com
VERSION=3.1.0.17256-optix1

BUILD=${VERSION##*.}

function run ()
{
    docker run --rm -v $PWD/state/etc:/opt/networkoptix/mediaserver/etc -v $PWD/state/var:/opt/networkoptix/mediaserver/var -p 7001:7001 -ti mediaserver:$VERSION $CLOUD_HOST
}

function publish_state
{
    local state_file=mediaserver-state-$VERSION-$CLOUD_HOST-$(date +%Y-%m-%d).tar.gz

    tar -C state -czf $state_file etc var
    aws s3 cp $state_file s3://nxcloud-prod-internal-data/monitoring/$state_file
    rm $state_file

    echo "State is s3://nxcloud-prod-internal-data/monitoring/$state_file"
}

function build ()
{
    rm -rf build
    mkdir build

    (mkdir -p deb; cd deb; wget -N http://updates.hdwitness.com.s3.amazonaws.com/default/$BUILD/linux/nxwitness-server-${VERSION}-linux64.deb)
    (cd build; ar x ../deb/*$VERSION*.deb; tar xJf data.tar.xz; rm control.tar.gz data.tar.xz debian-binary)
}

function stage ()
{
    rm -rf stage

	mkdir -p stage/mediaserver stage/var/log

    cp -Rlf build/opt/networkoptix/mediaserver/bin/{mediaserver-bin,mediaserver}

    cp -Rl build/opt/networkoptix/mediaserver/{bin,lib} stage/mediaserver
}

main $@
