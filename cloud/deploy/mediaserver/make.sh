#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=mediaserver
VERSION=3.1.0.17256

function publish_state
{
    local state_file=mediaserver-state-$VERSION-$(date +%Y-%m-%d).tar.gz

    tar -C /opt/networkoptix/mediaserver -czf $state_file etc var
    aws s3 cp $state_file s3://nxcloud-prod-internal-data/monitoring/$state_file
    rm $state_file

    echo "State is s3://nxcloud-prod-internal-data/monitoring/$state_file"
}

function build ()
{
    rm -rf build
    mkdir build
    (cd build; ar x ../deb/*$VERSION*.deb; tar xJf data.tar.xz)
}

function stage ()
{
    rm -rf stage

	mkdir -p stage/mediaserver stage/var/log

    cp -Rlf build/opt/networkoptix/mediaserver/bin/{mediaserver-bin,mediaserver}

    cp -Rl build/opt/networkoptix/mediaserver/{bin,lib} stage/mediaserver
}

main $@
