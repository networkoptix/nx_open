#!/bin/bash
set -x

export LOG=/var/log/bpi-upgrade.log
exec 1<&- #< Close stdout fd.
exec 2<&- #< Close stderr fd.
exec 1<>$LOG #< Open stdout as $LOG file for reading and writing.
exec 2>&1 #< Redirect stderr to stdout.

echo "Starting upgrade ..."
COMPANY_NAME="${deb.customization.company.name}"
MEDIASERVER_DIR="/opt/$COMPANY_NAME/mediaserver"
export DISTRIB="${artifact.name.server}"

update()
{
    cp "$DISTRIB.tar.gz" /tmp/
    rm -rf "$MEDIASERVER_DIR/lib" "$MEDIASERVER_DIR"/bin/core*
    mkdir -p "./$DISTRIB"
    tar xfv "$DISTRIB.tar.gz" -C "./$DISTRIB"
    cp -rf "./$DISTRIB"/* /

    if [[ "${box}" == "bpi" ]]; then
        # Avoid grabbing libstdc++ from mediaserver lib folder.
        export LD_LIBRARY_PATH=

        export DATAPART=/dev/mmcblk0p2
        export MNTDATAPART=/mnt/data
        export BOOTPART=/dev/mmcblk0p1
        export MNTBOOTPART=/mnt/boot

        mkdir -p $MNTBOOTPART
        mount -t vfat $BOOTPART $MNTBOOTPART
        cp -f /root/tools/uboot/* $MNTBOOTPART
        umount $BOOTPART
        rm -rf $MNTBOOTPART

        LIBVDPAU=$(dpkg -l |grep libvdpau |grep 0.4.1 |awk '{print $3}')
        [ -z "$LIBVDPAU" ] && dpkg -i /opt/deb/libvdpau/*.deb

        FONTCONFIG=$(dpkg -l |grep fontconfig |grep 2.11 |awk '{print $3}')
        [ -z "$FONTCONFIG" ] && dpkg -i /opt/deb/fontconfig/*.deb

        FONTS=$(dpkg -l |grep fonts-takao-mincho |awk '{print $3}')
        [ -z "$FONTS" ] && dpkg -i /opt/deb/fonts-takao-mincho/*.deb

        touch /dev/cedar_dev
        chmod 777 /dev/disp
        chmod 777 /dev/cedar_dev
        usermod -aG video root

        mkdir -p $MNTDATAPART
        mount -t ext4 $DATAPART $MNTDATAPART
        cp -f /opt/$COMPANY_NAME/mediaserver/var/ecs_static.sqlite /root/tools/nx/ecs_static.sqlite
        rm -rf $MNTDATAPART/opt/$COMPANY_NAME
        cp -rf ./$DISTRIB/* $MNTDATAPART
        mkdir -p $MNTDATAPART/opt/$COMPANY_NAME/mediaserver/var
        cp -f /root/tools/nx/ecs_static.sqlite $MNTDATAPART/opt/$COMPANY_NAME/mediaserver/var
        CONF_FILE="$MNTDATAPART/opt/$COMPANY_NAME/mediaserver/etc/mediaserver.conf"
        grep -q "statisticsReportAllowed=true" $CONF_FILE \
            && echo "statisticsReportAllowed=true" >> $CONF_FILE
        umount $DATAPART
        /etc/init.d/nx1boot upgrade
        rm -rf "./$DISTRIB"
        sync
    fi
    # TODO: Add errorlevel handling.
    rm "/tmp/$DISTRIB.tar.gz"
}

get_port_pid() # port
{
    local PORT="$1"
    netstat -tpan |grep "$PORT" |grep 'LISTEN' |awk '{print $NF}' |grep -o '[0-9]\+'
}

# Output nothing if not found.
get_another_server_pid() # port_pid
{
    local PORT_PID="$1"
    local RESULT=$(ps "$PORT_PID" |grep "/opt/$COMPANY_NAME/mediaserver")
    # $RESULT may contain spaces or \n, in this case we need to convert it to an empty string.
    echo "${RESULT// }"
}

/etc/init.d/cron stop
/etc/init.d/$COMPANY_NAME-mediaserver stop
update
if [ "${box}" = "bpi" ]; then
    reboot
    exit 0
fi

/etc/init.d/$COMPANY_NAME-mediaserver start

# TODO: #mshevchenko: Gone in 3.1.
/etc/init.d/cron start

sleep 3

# Determining Server port
SERVER_PORT=$(cat /opt/$COMPANY_NAME/mediaserver/etc/mediaserver.conf \
    |grep port= |grep -o '[0-9]\+')

# Determining the process that occupies Server port.
PORT_PID=$(get_port_process "$SERVER_PORT")

# Checking if the process belongs to our server. Netstat always shows process name like
# "xxxxx/mediaserver", but it may be the mediaserver of a different customization.
SERVER_PID=$(get_another_server_pid "$PORT_PID")
while [ -z "$SERVER_PID" ]; do
    echo "Restarting $COMPANY_NAME-mediaserver" >>"/opt/$COMPANY_NAME/mediaserver/var/log/update.log"
    /etc/init.d/$COMPANY_NAME-mediaserver stop
    kill -9 "$PORT_PID"
    /etc/init.d/$COMPANY_NAME-mediaserver start
    sleep 3
    PORT_PID=$(get_port_process "$SERVER_PORT")
    SERVER_PID=$(get_another_server_pid "$PORT_PID")
done
