#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=traffic_relay

function stage()
{
    rm -rf stage
    check_vms_dirs

	mkdir -p stage/traffic_relay/bin stage/traffic_relay/lib stage/qt/lib stage/qt/bin
	cp -rl $environment/packages/linux-x64/qt-$QT_VERSION/lib/* stage/qt/lib
	cp -rl $environment/packages/linux-x64/qt-$QT_VERSION/plugins/sqldrivers stage/qt/bin

	cp -rl $NX_VMS_DIR/build_environment/target/bin/$BUILD_CONFIGURATION/traffic_relay stage/traffic_relay/bin/traffic_relay
	cp -rl $NX_VMS_DIR/build_environment/target/lib/$BUILD_CONFIGURATION/* stage/traffic_relay/lib
}

main $@
