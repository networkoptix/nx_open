#!/bin/bash
set -x #< Log each command.
set -u #< Prohibit undefined variables.
set -e #< Exit on any error.

# Redirect all output of this script to the log file.
LOG_FILE="/var/log/bpi-upgrade.log"
exec 1<&- #< Close stdout fd.
exec 2<&- #< Close stderr fd.
exec 1<>"$LOG_FILE" #< Open stdout as $LOG_FILE for reading and writing.
exec 2>&1 #< Redirect stderr to stdout.

FAILURE_FLAG="/var/log/bpi-upgrade-failed.flag"
CUSTOMIZATION="${deb.customization.company.name}"
INSTALL_PATH="opt/$CUSTOMIZATION"
MEDIASERVER_PATH="$INSTALL_PATH/mediaserver"
DISTRIB="${artifact.name.server}"
STARTUP_SCRIPT="/etc/init.d/$CUSTOMIZATION-mediaserver"
TAR_FILE="./$DISTRIB.tar.gz"

# Call the specified command after mounting dev, setting MNT to the mount point.
# ATTENTION: The command is called without "exit-on-error".
callMounted() # filesystem dev MNT cmd [args...]
{
    local FILESYSTEM_callMounted="$1"; shift
    local DEV_callMounted="$1"; shift
    local MNT="$1"; shift

    mkdir -p "$MNT"
    umount "$DEV_callMounted" || true #< Unmount in case it was mounted.
    mount -t "$FILESYSTEM_callMounted" "$DEV_callMounted" "$MNT"

    local RESULT_callMounted=0
    "$@" || RESULT_callMounted=$? #< Called without "exit-on-error" due of "||".

    # Finally, unmount and return the original result of the called function.
    umount "$DEV_callMounted"
    rm -rf "$MNT"
    return $RESULT_callMounted
}

# ATTENTION: Intended to be called without "exit-on-error".
# [in] MNT
copyToBootPartition()
{
    cp -f "/root/tools/uboot"/* "$MNT" || true #< When cp omits dirs, it produces an error.
}

# ATTENTION: Intended to be called without "exit-on-error".
# [in] MNT
copyToDataPartition()
{
    rm -rf "$MNT/$INSTALL_PATH" || true

    # Unpack the distro to sdcard.
    tar xfv "$TAR_FILE" -C "$MNT/" || return $?

    mkdir -p "$MNT/$MEDIASERVER_PATH/var" || return $?
    cp -f "/root/tools/nx/ecs_static.sqlite" "$MNT/$MEDIASERVER_PATH/var/" || return $?

    local CONF_FILE="$MNT/$MEDIASERVER_PATH/etc/mediaserver.conf"
    local CONF_CONTENT="statisticsReportAllowed=true"
    if grep -q "^$CONF_CONTENT$" "$CONF_FILE"; then
        echo "$CONF_CONTENT" >>"$CONF_FILE" || return $?
    fi
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
    rm -rf "../$DISTRIB.zip" || true  #< Already unzipped, so remove .zip to save space in "/tmp".
    rm -rf "/$MEDIASERVER_PATH/lib" "/$MEDIASERVER_PATH/bin"/core* || true
    tar xfv "$TAR_FILE" -C / #< Extract the distro to the root.


  CIFSUTILS=$(dpkg --get-selections | grep -v deinstall | grep cifs-utils | awk '{print $1}')
  if [ -z "$CIFSUTILS" ]; then
      dpkg -i cifs-utils/*.deb
  fi

    if [[ "${box}" == "bpi" ]]; then
        # Avoid grabbing libstdc++ from mediaserver lib folder.
        export LD_LIBRARY_PATH=""

        cp -f "/$MEDIASERVER_PATH/var/ecs_static.sqlite" "/root/tools/nx/"

        callMounted vfat "/dev/mmcblk0p1" "/mnt/boot" copyToBootPartition

        installDeb libvdpau 0.4.1
        installDeb fontconfig 2.11
        installDeb fonts-takao-mincho ""

        touch "/dev/cedar_dev"
        chmod 777 "/dev/disp"
        chmod 777 "/dev/cedar_dev"
        usermod -aG video root

        callMounted ext4 "/dev/mmcblk0p2" "/mnt/data" copyToDataPartition

        /etc/init.d/nx1boot upgrade
    fi
}

getPidWhichUsesPort() # port
{
    local PORT="$1"
    netstat -tpln |grep ":$PORT\s" |head -n 1 |awk '{print $NF}' |grep -o '[0-9]\+'
}

# Output nothing if the specified pid does not belong to a mediaserver; otherwise, output the pid.
checkMediaserverPid() # pid
{
    local PID="$1"

    if [ ! -z $(ps "$PID" |grep "/$MEDIASERVER_PATH") ]; then
        echo "$PID"
    fi
}

restartMediaserver()
{
    local MEDIASERVER_PORT=$(cat "/$MEDIASERVER_PATH/etc/mediaserver.conf" \
        |grep '^port=[0-9]\+$' |sed 's/port=//')

    # If the mediaserver cannot start because another process uses its port, kill it and restart.
    while true; do
        "$STARTUP_SCRIPT" start || true #< If not started, the loop will try again.
        sleep 3

        local PID_WHICH_USES_PORT=$(getPidWhichUsesPort "$MEDIASERVER_PORT")
        local MEDIASERVER_PID=$(checkMediaserverPid "$PID_WHICH_USES_PORT")
        if [ ! -z "$MEDIASERVER_PID" ]; then
            echo "Upgraded mediaserver is up and running with pid $MEDIASERVER_PID at port $MEDIASERVER_PORT"
            break
        fi

        echo "Another process (pid $PID_WHICH_USES_PORT) uses port $MEDIASERVER_PORT:" \
            "killing it and restarting $CUSTOMIZATION-mediaserver"

        # Just in case - the mediaserver should not be running by now.
        "$STARTUP_SCRIPT" stop || true

        kill -9 "$PID_WHICH_USES_PORT" || true
    done
}

main()
{
    rm -rf "$FAILURE_FLAG" || true

    echo "Starting VMS upgrade..."

    "$STARTUP_SCRIPT" stop || true # If not stopped, try upgrading as is.

    upgradeVms
    echo "VMS upgrade succeeded"

    sync

    if [ "${box}" = "bpi" ]; then
        reboot
        exit 0
    fi

    restartMediaserver
}

onExit() # Called on exit via trap.
{
    local RESULT=$?

    set +e #< Disable stopping on errors.

    if [ $RESULT != 0 ]; then
        touch "$FAILURE_FLAG" || echo "ERROR: Unable to create flag file $FAILURE_FLAG"
        echo "ERROR: VMS upgrade script failed with exit status $RESULT; created $FAILURE_FLAG"
    fi
    return $RESULT
}

trap onExit EXIT
main "$@"
