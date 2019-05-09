#!/bin/bash

SERVER_DEB_FILE="@server_distribution_name@.deb"
COMPANY_NAME="@deb.customization.company.name@"
WITH_ROOT_TOOL="@withRootTool@"

osBasename() {
    local -r NAME=$(cat /etc/os-release |grep -e '^ID=' |sed 's/^ID=\(.*\)$/\1/')

    local -rA BASENAMES=(
        [debian]=debian
        [ubuntu]=ubuntu
        [raspbian]=debian
    )

    echo ${BASENAMES[$NAME]}
}

osVersion() {
    local -r VERSION=$(cat /etc/os-release |grep -e '^VERSION_ID=' |sed 's/^VERSION_ID="\(.*\)"/\1/')

    echo $VERSION
}

osMajorVersion() {
    local -r VERSION=$(osVersion)

    echo ${VERSION%%.*}
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

deleteObsoleteFiles()
{
    local -r DIR="/opt/$COMPANY_NAME"
    rm "$DIR/version.txt" &>/dev/null #< No longer present in 4.0.
    rm "$DIR/build_info.txt" &>/dev/null #< In 4.0 resides inside "mediaserver" dir.
    rm "$DIR/specific_features.txt" &>/dev/null #< In 4.0 resides inside "mediaserver" dir.
    rm "$DIR/installation_info.json" &>/dev/null #< Will be generated on first Server start.
}

ensureDependencyInstalled()
{
    local -r OS_DIRNAME=$(osBasename)$(osMajorVersion)

    while (( $# > 0 ))
    do
        local NAME=$1;shift

        [[ $(dpkg -l |grep -e "${NAME}\s" |grep -e '^.i' |awk '{print $2}') ]] && continue

        [[ -d "dependencies/${OS_DIRNAME}/${NAME}" ]] || continue

        installDeb "dependencies/${OS_DIRNAME}/${NAME}"/*.deb
    done
}

update()
{
    deleteObsoleteFiles

    export DEBIAN_FRONTEND=noninteractive

    ensureDependencyInstalled cifs-utils
    installDeb "$SERVER_DEB_FILE"

    # TODO: #alevenkov Consider moving this block to postinst to enable rpi camera after a clean
    # installation (this script works only when upgrading). Note the reboot command.
    if grep '^Hardware.*:.*BCM2835[[:space:]]*$' /proc/cpuinfo &>/dev/null
    then
        bash nx_rpi_cam_setup.sh
        reboot
        exit 0
    fi
}

if [ -n "$1" ]
then
    update >>$1 2>&1
else
    update 2>&1
fi
