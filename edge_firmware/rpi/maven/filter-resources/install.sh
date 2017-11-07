#!/bin/bash
set -e #< Exit on any error.
set -u #< Prohibit undefined variables.

configure()
{
    BOX="@box@"
    CUSTOMIZATION="@deb.customization.company.name@"
    INSTALL_PATH="opt/$CUSTOMIZATION"
    MEDIASERVER_PATH="$INSTALL_PATH/mediaserver"
    LITE_CLIENT_PATH="$INSTALL_PATH/lite_client"
    LOGS_DIR="/$MEDIASERVER_PATH/var/log"
    FAILURE_FLAG="$LOGS_DIR/vms-upgrade-failed.flag"
    LOG_FILE="$LOGS_DIR/vms-upgrade.log"
    DISTRIB="@artifact.name.server@"
    STARTUP_SCRIPT="/etc/init.d/$CUSTOMIZATION-mediaserver"
	INSTALLER_DIR="$(dirname "$0")"
    TAR_FILE="$INSTALLER_DIR/$DISTRIB.tar.gz"
	ZIP_FILE="$INSTALLER_DIR/../$DISTRIB.zip"
    TOOLS_DIR="/root/tools/$CUSTOMIZATION"
}

checkRunningUnderRoot()
{
    if [ "$(id -u)" != "0" ]; then
        echo "ERROR: $0 should be run under root" >&2
        exit 1
    fi
}

# Redirect all output of this script to the log file.
redirectOutput() # log_file
{
    local -r LOG_FILE="$1"
    echo "$0: All further output goes to $LOG_FILE"

    local -i RESULT

    mkdir -p "$(dirname "$LOG_FILE")" \
        || ( RESULT=$?; echo "ERROR: Cannot create dir for log file $LOG_FILE" >&2; exit $RESULT )

    exec 1<&- \
        || ( RESULT=$?; echo "ERROR: Cannot close stdout fd" >&2; exit $RESULT )
    exec 1<>"$LOG_FILE" \
        || ( RESULT=$?; echo "ERROR: Cannot redirect stdout to $LOG_FILE" >&2; exit $RESULT )
    exec 2<&- \
        || ( RESULT=$?; echo "ERROR: Cannot close stderr fd" >&2; exit $RESULT )
    exec 2>&1 \
        || ( RESULT=$?; echo "ERROR: Cannot redirect stderr to stdout" `# >stdout`; exit $RESULT )
}

# Call the specified command after mounting dev, setting MNT to the mount point.
# ATTENTION: The command is called without "exit-on-error".
callMounted() # filesystem dev MNT cmd [args...]
{
    local -r FILESYSTEM_callMounted="$1"; shift
    local -r DEV_callMounted="$1"; shift
    local -r MNT="$1"; shift

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
    # Clean up sdcard from potentially unwanted files from previous installations.
    rm -rf "$MNT/$INSTALL_PATH" || true
    rm -rf "$MNT/opt/deb"

    # Unpack the distro to sdcard.
    tar xfv "$TAR_FILE" -C "$MNT/" || return $?

    # Copy backed-up license db from HDD to SD card.
    mkdir -p "$MNT/$MEDIASERVER_PATH/var" || return $?
    cp -f "$TOOLS_DIR/ecs_static.sqlite" "$MNT/$MEDIASERVER_PATH/var/" || true

    local CONF_FILE="$MNT/$MEDIASERVER_PATH/etc/mediaserver.conf"
    local CONF_CONTENT="statisticsReportAllowed=true"
    if grep -q "^$CONF_CONTENT$" "$CONF_FILE"; then
        echo "$CONF_CONTENT" >>"$CONF_FILE" || return $?
    fi
}

# Install all deb packages from /opt/deb/<package>/, but do nothing if the directory is missing, or
# the specified version of a deb package with the same name as the directory is already installed.
installDebs() # package [version]
{
    local -r PACKAGE="$1"; shift
    local VERSION=""
    if [ $# -ge 2 ]; then
        VERSION="$2"
    fi

    local -r DEB_DIR="/opt/deb/$PACKAGE"
    if [ ! -d "$DEB_DIR" ]; then # The deb is missing from /opt/deb.
        return
    fi

    local -r INSTALLED_VERSION=$(dpkg -l |grep "$PACKAGE" |grep "$VERSION" |awk '{print $3}')
    if [ -z "$INSTALLED_VERSION" ]; then
        dpkg -i "$DEB_DIR"/*.deb
    fi
}

upgradeVms()
{
    rm -rf "$ZIP_FILE" || true  #< Already unzipped, so remove .zip to save space in "/tmp".

    # Clean up potentially unwanted files from previous installations.
    rm -rf "/$MEDIASERVER_PATH/lib" "/$MEDIASERVER_PATH/bin" || true
    rm -rf "/opt/deb"
    rm -rf "/$LITE_CLIENT_PATH"

    tar xfv "$TAR_FILE" -C / #< Extract the distro to the root.

    installDebs cifs-utils

    if [ "$BOX" = "bpi" ]; then
        # Avoid grabbing libstdc++ from mediaserver lib folder.
        export LD_LIBRARY_PATH=""

        # Backup current license db if exists.
        cp -f "/$MEDIASERVER_PATH/var/ecs_static.sqlite" "$TOOLS_DIR" || true

        callMounted vfat "/dev/mmcblk0p1" "/mnt/boot" copyToBootPartition

        installDebs libvdpau 0.4.1
        installDebs fontconfig 2.11
        installDebs fonts-takao-mincho
        installDebs fonts-baekmuk
        installDebs fonts-arphic-ukai
        installDebs fonts-thai-tlwg

        touch "/dev/cedar_dev"
        chmod 777 "/dev/disp"
        chmod 777 "/dev/cedar_dev"
        usermod -aG video root

        callMounted ext4 "/dev/mmcblk0p2" "/mnt/data" copyToDataPartition

        /etc/init.d/nx1boot upgrade
    fi

    rm -rf "/opt/deb" #< Delete deb packages to free some space.
}

getPidWhichUsesPort() # port
{
    local PORT="$1"
    netstat -tpln |grep ":$PORT\s" |head -n 1 |awk '{print $NF}' |grep -o '[0-9]\+'
}

# Output nothing if pid does not belong to a mediaserver or is empty; otherwise, output the pid.
checkMediaserverPid() # pid
{
    local PID="$1"

    if [ ! -z "$PID" ] && [ ! -z "$(ps "$PID" |grep "/$MEDIASERVER_PATH")" ]; then
        echo "$PID"
    fi
}

restartMediaserver()
{
    # If the mediaserver cannot start because another process uses its port, kill it and restart.
    while true; do
        "$STARTUP_SCRIPT" start || true #< If not started, the loop will try again.
        sleep 3

		# NOTE: mediaserver.conf may not exist before "start", thus, checking the port after it.
        local MEDIASERVER_PORT=$(cat "/$MEDIASERVER_PATH/etc/mediaserver.conf" \
            |grep '^port=[0-9]\+$' |sed 's/port=//')

        local PID_WHICH_USES_PORT=$(getPidWhichUsesPort "$MEDIASERVER_PORT")
        local MEDIASERVER_PID=$(checkMediaserverPid "$PID_WHICH_USES_PORT")

        if [ ! -z "$MEDIASERVER_PID" ]; then
            echo "Upgraded mediaserver is up and running with pid $MEDIASERVER_PID at port $MEDIASERVER_PORT"
            break
        fi

        if [ ! -z "$PID_WHICH_USES_PORT" ]; then
            echo "Another process (pid $PID_WHICH_USES_PORT) uses port $MEDIASERVER_PORT:" \
                "killing it and restarting $CUSTOMIZATION-mediaserver"

            # Just in case - the mediaserver should not be running by now.
            "$STARTUP_SCRIPT" stop || true

            kill -9 "$PID_WHICH_USES_PORT" || true
        fi
    done
}

main()
{
    configure
    checkRunningUnderRoot

    if [ $# = 0 ] || ( [ "$1" != "-v" ] && [ "$1" != "--verbose" ] ); then
        redirectOutput "$LOG_FILE"
    fi

    set -x #< Log each command.
    configure #< If the output was redirected, do it again to log the values.

    rm -rf "$FAILURE_FLAG" || true

    echo "Stopping mediaserver..."
    "$STARTUP_SCRIPT" stop || true # If not stopped, try upgrading as is.

    echo "Starting VMS upgrade..."
    upgradeVms
    echo "VMS upgrade succeeded"

    sync

    if [ "$BOX" = "bpi" ]; then
        reboot
        exit 0
    fi

    restartMediaserver
}

onExit() # Called on exit via trap.
{
    local -r -i RESULT=$?
    if [ $RESULT != 0 ]; then
        local -r MESSAGE="ERROR: VMS upgrade script failed with exit status $RESULT"
        mkdir -p "$(dirname "$FAILURE_FLAG")" \
            || echo "ERROR: Unable to create dir for flag file $FAILURE_FLAG" >&2
        if touch "$FAILURE_FLAG"; then
            echo "$MESSAGE; created $FAILURE_FLAG" >&2
        else
            echo "ERROR: Unable to create flag file $FAILURE_FLAG" >&2
            echo "$MESSAGE" >&2
        fi
    fi
    return $RESULT
}

trap onExit EXIT
main "$@"
