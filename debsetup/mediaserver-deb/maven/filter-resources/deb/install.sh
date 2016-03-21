#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
BETA=

if [[ "${beta}" == "true" ]]; then 
  BETA="-beta" 
fi 

DISTRIB=${installer.name}-mediaserver-${release.version}.${buildNumber}-${arch}-${build.configuration}$BETA.deb

update () {
    export DEBIAN_FRONTEND=noninteractive
    dpkg -i $DISTRIB
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
