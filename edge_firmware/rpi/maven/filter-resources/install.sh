#!/bin/bash

set -x

export NX1UPGRADELOG=/var/log/nx1upgrade.log
# Close STDOUT file descriptor
exec 1<&-
# Close STDERR FD
exec 2<&-

# Open STDOUT as $NX1UPGRADELOG file for read and write.
exec 1<>$NX1UPGRADELOG

# Redirect STDERR to STDOUT
exec 2>&1

echo "Starting upgrade ..."
COMPANY_NAME=${deb.customization.company.name}
export DISTRIB=${artifact.name.server}

update () {
  cp $DISTRIB.tar.gz /tmp
  rm -Rf /opt/networkoptix/mediaserver/lib
  mkdir -p ./$DISTRIB
  tar xfv $DISTRIB.tar.gz -C ./$DISTRIB
  cp -Rf ./$DISTRIB/* /
  if [[ "${box}" == "bpi" ]]; then 
    #avoid grabbing libstdc++ from mediaserver lib folder
    export LD_LIBRARY_PATH=
    export DATAPART=/dev/mmcblk0p2
    export MNTDATAPART=/mnt/data
    export BOOTPART=/dev/mmcblk0p1
    export MNTBOOTPART=/mnt/boot

    mkdir -p $MNTBOOTPART
    mount -t vfat $BOOTPART $MNTBOOTPART
    cp -f /root/tools/uboot/* $MNTBOOTPART
    umount $BOOTPART
    rm -Rf $MNTBOOTPART

    #making /tmp tmpfs (hosted in RAM)
    tmp=$(df -h | grep "/tmp")
    if [ -z $tmp ]; then echo "tmpfs /tmp tmpfs rw,nosuid,nodev" | tee -a /etc/fstab; fi
    
    LIBVDPAU=$(dpkg -l | grep libvdpau | grep 0.4.1 | awk '{print $3}')
    if [ -z $LIBVDPAU ]; then dpkg -i /opt/deb/libvdpau/*.deb; fi

    FONTCONFIG=$(dpkg -l | grep fontconfig | grep 2.11 | awk '{print $3}')
    if [ -z $FONTCONFIG ]; then dpkg -i /opt/deb/fontconfig/*.deb; fi

    touch /dev/cedar_dev
    chmod 777 /dev/disp
    chmod 777 /dev/cedar_dev
    usermod -aG video root

    mkdir -p $MNTDATAPART
    mount -t ext4 $DATAPART $MNTDATAPART
    cp -f /opt/$COMPANY_NAME/mediaserver/var/ecs_static.sqlite /root/tools/nx/ecs_static.sqlite
    rm -Rf $MNTDATAPART/opt/$COMPANY_NAME
    cp -Rf ./$DISTRIB/* $MNTDATAPART
    mkdir -p $MNTDATAPART/opt/$COMPANY_NAME/mediaserver/var
    cp -f /root/tools/nx/ecs_static.sqlite $MNTDATAPART/opt/$COMPANY_NAME/mediaserver/var
    CONF_FILE="$MNTDATAPART/opt/$COMPANY_NAME/mediaserver/etc/mediaserver.conf"
    grep -q "statisticsReportAllowed=true" $CONF_FILE \
        && echo "statisticsReportAllowed=true" >> $CONF_FILE
    umount $DATAPART
    /etc/init.d/nx1boot upgrade
    rm -Rf ./$DISTRIB
    sync    
  fi
  # TODO: add errorlevel handling
  rm /tmp/$DISTRIB.tar.gz
}

/etc/init.d/cron stop
/etc/init.d/$COMPANY_NAME-mediaserver stop
update
if [[ "${box}" == "bpi" ]]; then reboot && exit 0; fi
/etc/init.d/$COMPANY_NAME-mediaserver start
/etc/init.d/cron start

sleep 3
# Determining Server Port
SERVER_PORT=`cat /opt/$COMPANY_NAME/mediaserver/etc/mediaserver.conf | grep port= | grep -o '[0-9]\+'`
# Determining process that occupies Server Port
PORT_PROCESS=`netstat -tpan | grep $SERVER_PORT | grep LISTEN | awk {'print $NF'} | grep -o '[0-9]\+'`
# Checking if the process belongs to our server. Netstat always shows process name like "xxxxx/mediaserver", but it may be mediaserver of a different customization
SERVER_PROCESS=`ps $PORT_PROCESS | grep $'/opt/'$COMPANY_NAME'/mediaserver'`
# Note that $SERVER_PROCESS may contain spaces or \n in this case we need to convert it to empty string
while [ -z "${SERVER_PROCESS// }" ]; do
  echo "Restarting "$COMPANY_NAME"-mediaserver" >> /opt/$COMPANY_NAME/mediaserver/var/log/update.log
  /etc/init.d/$COMPANY_NAME-mediaserver stop
  kill -9 $PORT_PROCESS
  /etc/init.d/$COMPANY_NAME-mediaserver start
  sleep 3
  PORT_PROCESS=`netstat -tpan | grep $SERVER_PORT | grep LISTEN | awk {'print $NF'} | grep -o '[0-9]\+'` 
  SERVER_PROCESS=`ps $PORT_PROCESS | grep $'/opt/'$COMPANY_NAME'/mediaserver'`  
done
