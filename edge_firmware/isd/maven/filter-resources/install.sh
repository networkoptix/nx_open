#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
export DISTRIB=${artifact.name.server}

update () {
    /etc/init.d/S99$COMPANY_NAME-mediaserver stop
    rm -Rf /usr/local/apps/${deb.customization.company.name}/mediaserver/lib
    tar xfv $DISTRIB.tar.gz -C /
    /etc/init.d/S99$COMPANY_NAME-mediaserver start
}

if [ "$1" != "" ]
then
    update >> $1 2>&1
else
    update 2>&1
fi
