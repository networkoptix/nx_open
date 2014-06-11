#!/bin/bash

DISTRIB=$COMPANY_NAME-mediaserver-${release.version}.${buildNumber}-${arch}-${build.configuration}-beta

function update () {
    export DEBIAN_FRONTEND=noninteractive
    dpkg -i $DISTRIB
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
