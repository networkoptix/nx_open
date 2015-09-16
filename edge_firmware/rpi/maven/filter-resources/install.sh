#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
BETA=

if [[ "${beta}" == "true" ]]; then 
  BETA="-beta" 
fi 
export DISTRIB=$COMPANY_NAME-mediaserver-${box}-${release.version}.${buildNumber}$BETA

update () {
  /etc/init.d/cron stop
  /etc/init.d/$COMPANY_NAME-mediaserver stop
  cp /opt/${deb.customization.company.name}/mediaserver/etc/mediaserver.conf /tmp/mediaserver.conf
  cp $DISTRIB.tar.gz /tmp
  tar xfv $DISTRIB.tar.gz -C /
  if [[ "${box}" == "bpi" ]]; then /etc/init.d/nx1upgrade; fi
  # TODO: add errorlevel handling
  cat /tmp/mediaserver.conf >> /opt/${deb.customization.company.name}/mediaserver/etc/mediaserver.conf  
  /etc/init.d/$COMPANY_NAME-mediaserver start
  /etc/init.d/cron start
  rm /tmp/$DISTRIB.tar.gz
}

if [ "$1" != "" ]
then
  update >> $1 2>&1
else
  update 2>&1
fi
