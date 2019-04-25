#!/bin/bash

DISTRIB="@server_distribution_name@.deb"
COMPANY_NAME="@deb.customization.company.name@"
WITH_ROOT_TOOL="@withRootTool@"
TARGET_DEVICE="@targetDevice@"

RELEASE_YEAR=$(lsb_release -a |grep "Release:" |awk {'print $2'} |awk -F  "." '/1/ {print $1}')

installDeb()
{
    local -r DEB="$1"

    if [ "$WITH_ROOT_TOOL" = true ]
    then
        local -r ROOT_TOOL_BINARY="/opt/$COMPANY_NAME/mediaserver/bin/root-tool-bin"
        if [ -f "${ROOT_TOOL_BINARY}" ]
        then #< It must be (4.0+) update.
            ${ROOT_TOOL_BINARY} install "$DEB"
        else #< It must be (<=3.2)  --> (4.0+) update.
            dpkg -i "$DEB"
        fi
    else
        dpkg -i "$DEB"
    fi
}

deleteObsoleteFiles()
{
    local -r DIR="/opt/$COMPANY_NAME"
    rm "$DIR/version.txt" #< No longer present in 4.0.
    rm "$DIR/build_info.txt" #< In 4.0 resides inside "mediaserver" dir.
    rm "$DIR/specific_features.txt" #< In 4.0 resides inside "mediaserver" dir.
    rm "$DIR/installation_info.json" #< Will be generated on first Server start.
}

update()
{
    deleteObsoleteFiles

    export DEBIAN_FRONTEND=noninteractive
    CIFSUTILS=$(dpkg -l |grep cifs-utils |grep ii |awk '{print $2}')
    if [ -z "$CIFSUTILS" ]
    then
        [ -d "ubuntu${RELEASE_YEAR}" ] && installDeb ubuntu${RELEASE_YEAR}/cifs-utils/*.deb
    fi
    installDeb "$DISTRIB"

    # TODO: #alevenkov Consider moving this block to postinst to enable rpi camera after a clean
    # installation (this script works only when upgrading). Note the reboot command.
    if grep '^Hardware.*:.*BCM2835[[:space:]]*$' /proc/cpuinfo &>/dev/null
    then
        bash nx_rpi_cam_setup.sh
        reboot
        exit 0
    fi
}

if [ "$1" != "" ]
then
    update >>$1 2>&1
else
    update 2>&1
fi
