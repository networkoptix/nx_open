#!/bin/bash

DISTRIB=${artifact.name.server}.deb

update () {
    export DEBIAN_FRONTEND=noninteractive
    dpkg -i $DISTRIB
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
