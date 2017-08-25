#!/bin/sh

CUSTOMIZATION="@deb.customization.company.name@"
DISTRIB="@artifact.name.server@"

INSTALL_DIR="/usr/local/apps/$CUSTOMIZATION"
STARTUP_SCRIPT="/etc/init.d/S99$CUSTOMIZATION-mediaserver"
TAR_FILE="./$DISTRIB.tar.gz"

upgradeVms()
{
    "$STARTUP_SCRIPT" stop
    rm -rf "$INSTALL_DIR/mediaserver/lib"
    tar xfv "$TAR_FILE" -C /
    "$STARTUP_SCRIPT" start
}

main()
{
    if [ "$1" != "" ]; then
        upgradeVms >>"$1" 2>&1
    else
        upgradeVms 2>&1
    fi
}

main "$@"
