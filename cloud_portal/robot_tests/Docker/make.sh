#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=mediaserver
VERSION=4.0.0.28077

function build ()
{
    rm -rf build
    mkdir build
    (cd build; ar x /home/kyle/Downloads/*$VERSION*.deb; tar xJf data.tar.xz)
}

function stage ()
{
    rm -rf stage

	mkdir -p stage/mediaserver stage/var/log

    cp -Rlf build/opt/networkoptix/mediaserver/bin/{mediaserver-bin,mediaserver}
    cp -Rl build/opt/networkoptix/mediaserver/{bin,lib,plugins} stage/mediaserver

#     cp -Rlf /home/kyle/develop/nx_vms/nx_cloud_deploy/mediaserver/build/opt/networkoptix/mediaserver/bin/{mediaserver-bin,mediaserver}
#    cp -Rl /home/kyle/develop/nx_vms/nx_cloud_deploy/mediaserver/build/opt/networkoptix/mediaserver/{bin,lib} stage/mediaserver
}

main $@
