#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
BETA=

if [[ "${beta}" == "true" ]]; then 
  BETA="-beta" 
fi 
DISTRIB=$COMPANY_NAME-mediaserver-${release.version}.${buildNumber}-${box}$BETA

update () {
    /etc/init.d/S99$COMPANY_NAME-mediaserver stop
    tar xfv $DISTRIB.tar.gz  --exclude='./usr/local/apps/networkoptix/mediaserver/etc/mediaserver.conf' -C /
    /etc/init.d/S99$COMPANY_NAME-mediaserver start
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
