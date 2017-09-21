#!/bin/bash

DISTRIB=${artifact.name.server}.deb

UBUNTU_DISTRIB=$(lsb_release -a | grep Codename | awk {'print $2'})

update () {
    export DEBIAN_FRONTEND=noninteractive
    CIFSUTILS=$(dpkg -l | grep cifs-utils | grep ii | awk '{print $2}')
    if [ -z "$CIFSUTILS" ]; then
        #TODO: check when newer Ubuntu distrib is out and add another line
        if [ "$UBUNTU_DISTRIB" == "trusty" ]; then dpkg -i ubuntu14/cifs-utils/*.deb; fi
        if [ "$UBUNTU_DISTRIB" == "xenial" ]; then dpkg -i ubuntu16/cifs-utils/*.deb; fi
    fi
    dpkg -i $DISTRIB
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
