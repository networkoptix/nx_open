#!/bin/bash

DISTRIB=${artifact.name.server}.deb

RELEASE_YEAR=$(lsb_release -a | grep "Release:" | awk {'print $2'} | awk -F  "." '/1/ {print $1}')

update () {
    export DEBIAN_FRONTEND=noninteractive
    CIFSUTILS=$(dpkg -l | grep cifs-utils | grep ii | awk '{print $2}')
    if [ -z "$CIFSUTILS" ]; then
        [ -d "ubuntu${RELEASE_YEAR}" ] && dpkg -i ubuntu${RELEASE_YEAR}/cifs-utils/*.deb
    fi
    dpkg -i $DISTRIB
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
