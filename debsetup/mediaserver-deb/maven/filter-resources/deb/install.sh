#!/bin/bash

DISTRIB=${artifact.name.server}.deb

update () {
    export DEBIAN_FRONTEND=noninteractive
    CIFSUTILS=$(dpkg --get-selections | grep -v deinstall | grep cifs-utils | awk '{print $1}')
    if [ -z "$CIFSUTILS" ]; then 
        dpkg -i cifs-utils/*.deb
    fi    
    dpkg -i $DISTRIB
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
