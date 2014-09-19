#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
BETA=

if [[ "${beta}" == "true" ]]; then 
  BETA="-beta" 
fi 
DISTRIB=$COMPANY_NAME-mediaserver-${box}-${release.version}.${buildNumber}$BETA

update () {
    /etc/init.d/$COMPANY_NAME-mediaserver stop
    tar xfv $DISTRIB.tar.gz --exclude='./opt/$COMPANY_NAME/mediaserver/etc/mediaserver.conf' -C /
    /etc/init.d/$COMPANY_NAME-mediaserver start
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
