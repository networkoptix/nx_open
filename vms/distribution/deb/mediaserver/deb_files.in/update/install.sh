#!/bin/bash

DISTRIB="@server_distribution_name@.deb"
COMPANY_NAME="@deb.customization.company.name@"
WITH_ROOT_TOOL="@withRootTool@"
TARGET_DEVICE="@targetDevice@"

RELEASE_YEAR=$(lsb_release -a |grep "Release:" |awk {'print $2'} |awk -F  "." '/1/ {print $1}')

is_rpi() {
    if [[ $TARGET_DEVICE == "linux_arm32" ]]
    then
        grep '^Hardware.*:.*BCM2835[[:space:]]*$' /proc/cpuinfo &>/dev/null
    else
        false
    fi
}

is_bananapi() {
    if [[ $TARGET_DEVICE == "linux_arm32" ]]
    then
        grep '^Hardware.*:.*sun8i[[:space:]]*$' /proc/cpuinfo &>/dev/null
    else
        false
    fi
}

hw_platform() {
    if is_rpi
    then
        echo "raspberryPi"
    elif is_bananapi
    then
        echo "bananaPi"
    else
        echo "unknown"
    fi
}

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

update()
{
    export DEBIAN_FRONTEND=noninteractive
    CIFSUTILS=$(dpkg -l |grep cifs-utils |grep ii |awk '{print $2}')
    if [ -z "$CIFSUTILS" ]
    then
        [ -d "ubuntu${RELEASE_YEAR}" ] && installDeb ubuntu${RELEASE_YEAR}/cifs-utils/*.deb
    fi
    installDeb "$DISTRIB"

    echo "{\n    \"hwPlatform\": \"$(hw_platform)\"\n}" >"/opt/$COMPANY_NAME/installation_info.json"

    if is_rpi
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
