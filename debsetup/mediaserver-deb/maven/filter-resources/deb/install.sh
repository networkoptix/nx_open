#!/bin/bash

DISTRIB=${artifact.name.server}.deb

RELEASE_YEAR=$(lsb_release -a | grep "Release:" | awk {'print $2'} | awk -F  "." '/1/ {print $1}')

install_deb () {
    if [ $(whoami) == root ]; then
        dpkg -i "$1"
    else
        /opt/${deb.customization.company.name}/mediaserver/bin/root_tools install "$1"
    fi
}

update () {
    export DEBIAN_FRONTEND=noninteractive
    CIFSUTILS=$(dpkg -l | grep cifs-utils | grep ii | awk '{print $2}')
    if [ -z "$CIFSUTILS" ]; then
        [ -d "ubuntu${RELEASE_YEAR}" ] && install_deb ubuntu${RELEASE_YEAR}/cifs-utils/*.deb
    fi
    install_deb $DISTRIB
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
