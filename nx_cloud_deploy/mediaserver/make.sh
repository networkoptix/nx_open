#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=mediaserver
VERSION=3.0.0.14670

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
