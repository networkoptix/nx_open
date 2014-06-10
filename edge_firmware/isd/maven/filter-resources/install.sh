#!/bin/bash

DISTRIB=${deb.customization.company.name}-mediaserver-${release.version}.${buildNumber}-${arch}-${build.configuration}-beta.deb

function update () {
    /etc/init.d/S99networkoptix-mediaserver stop
    tar xfv networkoptix-hdwitness-mediaserver-2.3.0.0-isd_s2.tar.gza -C /
    /etc/init.d/S99networkoptix-mediaserver start
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi