#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
BETA=

if [[ "${beta}" == "true" ]]; then 
  BETA="-beta" 
fi 
DISTRIB=$COMPANY_NAME-mediaserver-${box}-${release.version}.${buildNumber}$BETA

update () {
  /etc/init.d/cron stop
  /etc/init.d/$COMPANY_NAME-mediaserver stop
  tar xfv $DISTRIB.tar.gz --exclude='./opt/${deb.customization.company.name}/mediaserver/etc/mediaserver.conf' -C /
  if [[ "${box}" == "bpi" ]]; then /etc/init.d/upgrade; fi
  /etc/init.d/$COMPANY_NAME-mediaserver start
  /etc/init.d/cron start
}

if [ "$1" != "" ]
then
  update >> $1 2>&1
else
  update 2>&1
fi
