#!/bin/bash

DISTRIB="@server_distribution_name@.deb"
COMPANY_NAME="@deb.customization.company.name@"
WITH_ROOT_TOOL="@withRootTool@"

RELEASE_YEAR=$(lsb_release -a |grep "Release:" |awk {'print $2'} |awk -F  "." '/1/ {print $1}')

installDeb()
{
    local -r DEB="$1"
    local -r FORCE="$2"
    local ARGS="-i"

    if [ $FORCE = "force-conflicts" ]
    then
        ARGS="$ARGS --auto-deconfigure --force-conflicts"
    fi

    if [ "$WITH_ROOT_TOOL" = true ]
    then
        local -r ROOT_TOOL_BINARY="/opt/$COMPANY_NAME/mediaserver/bin/root-tool-bin"
        if [ -f "${ROOT_TOOL_BINARY}" ]
        then #< It must be 4.0+ update.
            ${ROOT_TOOL_BINARY} install "$DEB" "$FORCE"
        else #< It must be 3.2 --> 4.0 update.
            dpkg $ARGS "$DEB"
        fi
    else
        dpkg $ARGS "$DEB"
    fi
}

update()
{
    export DEBIAN_FRONTEND=noninteractive
    CIFSUTILS=$(dpkg -l |grep cifs-utils |grep ii |awk '{print $2}')
    if [ -z "$CIFSUTILS" ]
    then
        [ -d "ubuntu${RELEASE_YEAR}" ] && installDeb ubuntu${RELEASE_YEAR}/cifs-utils/*.deb
    fi
    installDeb "$DISTRIB" force-conflicts
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
