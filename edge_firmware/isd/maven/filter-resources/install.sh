#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
BETA=

if [[ "${beta}" == "true" ]]; then 
  BETA="-beta" 
fi 
DISTRIB=$COMPANY_NAME-mediaserver-${box}-${release.version}.${buildNumber}$BETA

update () {
    /etc/init.d/S99$COMPANY_NAME-mediaserver stop
    cp /opt/${deb.customization.company.name}/mediaserver/etc/mediaserver.conf /tmp/mediaserver.conf
    tar xfv $DISTRIB.tar.gz -C /
    cat /tmp/mediaserver.conf >> /opt/${deb.customization.company.name}/mediaserver/etc/mediaserver.conf     
    /etc/init.d/S99$COMPANY_NAME-mediaserver start
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
