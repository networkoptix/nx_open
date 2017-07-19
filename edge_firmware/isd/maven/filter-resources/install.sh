#!/bin/sh

CUSTOMIZATION=${deb.customization.company.name}
export DISTRIB=${artifact.name.server}

INSTALL_DIR="/usr/local/apps/$CUSTOMIZATION"

update()
{
    /etc/init.d/S99$CUSTOMIZATION-mediaserver stop
    rm -rf "$INSTALL_DIR/mediaserver/lib"
    tar xfv $DISTRIB.tar.gz -C /
    /etc/init.d/S99$CUSTOMIZATION-mediaserver start
}

if [ "$1" != "" ]; then
    update >>"$1" 2>&1
else
    update 2>&1
fi
