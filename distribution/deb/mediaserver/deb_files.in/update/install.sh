#!/bin/bash

DISTRIB="@server_distribution_name@.deb"

RELEASE_YEAR=$(lsb_release -a |grep "Release:" |awk {'print $2'} |awk -F  "." '/1/ {print $1}')

installDeb()
{
    if [ "@withRootTool@" = "true" ]
    then
        "/opt/@deb.customization.company.name@/mediaserver/bin/root_tool" install "$1"
    elif [ "$(whoami)" == root ]
    then
        dpkg -i "$1"
    fi
}

update()
{
    export DEBIAN_FRONTEND=noninteractive
    CIFSUTILS=$(dpkg -l |grep cifs-utils |grep ii |awk '{print $2}')
    if [ -z "$CIFSUTILS" ]; then
        [ -d "ubuntu${RELEASE_YEAR}" ] && installDeb ubuntu${RELEASE_YEAR}/cifs-utils/*.deb
    fi
    installDeb "$DISTRIB"
}

if [ "$1" != "" ]; then
    update >> $1 2>&1
else
    update 2>&1
fi
