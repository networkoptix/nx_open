#!/bin/bash
set -x #< Log each command.

# Redirect all output of this script to the log file.
LOG_FILE="/var/log/bpi-upgrade.log"
exec 1<&- #< Close stdout fd.
exec 2<&- #< Close stderr fd.
exec 1<>"$LOG_FILE" #< Open stdout as $LOG_FILE for reading and writing.
exec 2>&1 #< Redirect stderr to stdout.

FAILED_FLAG="/var/log/bpi-upgrade-failed.flag"
CUSTOMIZATION="${deb.customization.company.name}"
INSTALL_PATH="opt/$CUSTOMIZATION"
MEDIASERVER_PATH="$INSTALL_PATH/mediaserver"
DISTRIB="${artifact.name.server}"
STARTUP_SCRIPT="/etc/init.d/$CUSTOMIZATION-mediaserver"
TAR_FILE="./$DISTRIB.tar.gz"

copyFilesToBootPartition()
{
    local DEV="/dev/mmcblk0p1"
    local MNT="/mnt/boot"
    mkdir -p "$MNT" || return $?
    umount "$DEV" #< Unmount in case it was mounted; ignore errors.
    mount -t vfat "$DEV" "$MNT" || return $?
    cp -f "/root/tools/uboot"/* "$MNT" #< When cp omits dirs, it produces an error.
    umount "$DEV" || return $?
    rm -rf "$MNT" || return $?
}

copyFilesToDataPartition()
{
    local DEV="/dev/mmcblk0p2"
    local MNT="/mnt/data"
    mkdir -p "$MNT" || return $?
    umount "$DEV" #< Unmount in case it was mounted; ignore errors.
    mount -t ext4 "$DEV" "$MNT" || return $?
    cp -f "/$MEDIASERVER_PATH/var/ecs_static.sqlite" "/root/tools/nx/" || return $?
    rm -rf "$MNT/$INSTALL_PATH" || return $?

    # Unpack the distro to sdcard.
    tar xfv "$TAR_FILE" -C "$MNT/" || return $?

    mkdir -p "$MNT/$MEDIASERVER_PATH/var" || return $?
    cp -f "/root/tools/nx/ecs_static.sqlite" "$MNT/$MEDIASERVER_PATH/var/" || return $?
    local CONF_FILE="$MNT/$MEDIASERVER_PATH/etc/mediaserver.conf"
    local CONF_CONTENT="statisticsReportAllowed=true"
    grep -q "$CONF_CONTENT" "$CONF_FILE" \
        && { echo "$CONF_CONTENT" >>"$CONF_FILE" || return $?; }
    umount "$DEV" || return $?
}

installDeb() # package version
{
    local PACKAGE="$1"
    local VERSION="$2"

    local PRESENT=$(dpkg -l |grep "$PACKAGE" |grep "$VERSION" |awk '{print $3}')
    if [ -z "$PRESENT" ]; then
        dpkg -i "/opt/deb/$PACKAGE"/*.deb
    fi
}

upgradeVms()
{
    rm -rf "../$DISTRIB.zip" #< Already unzipped, so remove .zip to save space in "/tmp".
    rm -rf "/$MEDIASERVER_PATH/lib" "/$MEDIASERVER_PATH/bin"/core* #< Ignore errors.
    tar xfv "$TAR_FILE" -C / || return $? #< Extract the distro to the root.


    CIFSUTILS=$(dpkg --get-selections | grep -v deinstall | grep cifs-utils | awk '{print $1}')
    if [ -z "$CIFSUTILS" ]; then
        dpkg -i cifs-utils/*.deb
    fi

    if [[ "${box}" == "bpi" ]]; then
        # Avoid grabbing libstdc++ from mediaserver lib folder.
        export LD_LIBRARY_PATH=""

        copyFilesToBootPartition || return $?

        installDeb libvdpau 0.4.1 || return $?
        installDeb fontconfig 2.11 || return $?
        installDeb fonts-takao-mincho "" || return $?
        installDeb fonts-baekmuk "" || return $?
        installDeb fonts-arphic-ukai "" || return $?

        touch "/dev/cedar_dev" || return $?
        chmod 777 "/dev/disp" || return $?
        chmod 777 "/dev/cedar_dev" || return $?
        usermod -aG video root || return $?

        copyFilesToDataPartition || return $?

        /etc/init.d/nx1boot upgrade || return $?
    fi
}

getPidWhichUsesPort() # port
{
    local PORT="$1"
    netstat -tpan |grep "$PORT" |grep 'LISTEN' |awk '{print $NF}' |grep -o '[0-9]\+'
}

# Output nothing if the specified pid does not belong to a mediaserver; otherwise, output the pid.
getMediaserverPid() # pid
{
    local PID="$1"

    if [ ! -z $(ps "$PID" |grep "/$MEDIASERVER_PATH") ]; then
        echo "$PID"
    fi
}

upgradeVmsAndRestart()
{
    echo "Starting VMS upgrade..."

    "$STARTUP_SCRIPT" stop #< Ignore errors - if not stopped, try upgrading as is.

    upgradeVms
    local RESULT=$?
    if [ $RESULT != 0 ]; then
        echo "VMS upgrade failed with exit status $RESULT"
        return $RESULT;
    fi
    echo "VMS upgrade succeeded"

    sync

    if [ "${box}" = "bpi" ]; then
        reboot
        exit 0
    fi

    "$STARTUP_SCRIPT" start #< Ignore errors - if not started, try to restart below.

    sleep 3

    local SERVER_PORT=$(cat "/$MEDIASERVER_PATH/etc/mediaserver.conf" \
        |grep 'port=' |grep -o '[0-9]\+')

    # If the server cannot start because another process uses its port, kill it and restart.
    while true; do
        local PID_WHICH_USES_PORT=$(getPidWhichUsesPort "$SERVER_PORT")
        local SERVER_PID=$(getMediaserverPid "$PID_WHICH_USES_PORT")
        if [ ! -z "$SERVER_PID" ]; then
            echo "Upgraded mediaserver is up and running with pid $SERVER_PID at port $SERVER_PORT"
            break
        fi
        echo "Another process (pid $PID_WHICH_USES_PORT) uses port $SERVER_PORT:" \
            "killing it and restarting $CUSTOMIZATION-mediaserver"
        "$STARTUP_SCRIPT" stop #< Just in case - the server should not be running at this time.
        kill -9 "$PID_WHICH_USES_PORT"
        "$STARTUP_SCRIPT" start #< Ignore errors - if not started, the loop will try again.
        sleep 3
    done
}

main()
{
    rm -rf "$FAILED_FLAG"

    upgradeVmsAndRestart
    local RESULT=$?
    if [ $RESULT != 0 ]; then
        touch "$FAILED_FLAG"
    fi
    return $RESULT
}

main "$@"
