#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
BETA=

if [[ "${beta}" == "true" ]]; then 
  BETA="-beta" 
fi 
export DISTRIB=$COMPANY_NAME-mediaserver-${box}-${release.version}.${buildNumber}$BETA

update () {
  cp /opt/$COMPANY_NAME/mediaserver/etc/mediaserver.conf /tmp/mediaserver.conf
  cp $DISTRIB.tar.gz /tmp
  tar xfv $DISTRIB.tar.gz -C /
  if [[ "${box}" == "bpi" ]]; then /etc/init.d/nx1upgrade; fi
  # TODO: add errorlevel handling
  cat /tmp/mediaserver.conf >> /opt/$COMPANY_NAME/mediaserver/etc/mediaserver.conf  
  /etc/init.d/nx1boot upgrade  
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