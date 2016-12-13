#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
BETA=

if [[ "${beta}" == "true" ]]; then 
  BETA="-beta" 
fi 
export DISTRIB=$COMPANY_NAME-mediaserver-${box}-${release.version}.${buildNumber}$BETA

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

    LIBVDPAU=$(dpkg -l | grep libvdpau | grep 0.4.1)
    if [ -z $LIBVDPAU ]; then dpkg -i /opt/deb/libvdpau/*.deb; fi

    FONTCONFIG=$(dpkg -l | grep fontconfig | grep 2.11)
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
if [ "$1" != "" ]
then
  update >> $1 2>&1
else
  update 2>&1
fi
if [[ "${box}" == "bpi" ]]; then reboot; fi
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
  kill -9 $PORT_PROCESS
  /etc/init.d/$COMPANY_NAME-mediaserver start
  sleep 3
  PORT_PROCESS=`netstat -tpan | grep $SERVER_PORT | grep LISTEN | awk {'print $NF'} | grep -o '[0-9]\+'` 
  SERVER_PROCESS=`ps $PORT_PROCESS | grep $'/opt/'$COMPANY_NAME'/mediaserver'`  
done
