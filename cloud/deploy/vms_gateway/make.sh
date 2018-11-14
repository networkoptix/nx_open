#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=vms_gateway

function stage()
{
    rm -rf stage
    check_vms_dirs

	mkdir -p stage/vms_gateway/bin stage/vms_gateway/lib stage/qt/lib stage/qt/bin
	cp -rl $environment/packages/linux-x64/qt-$QT_VERSION/lib/* stage/qt/lib
	cp -rl $environment/packages/linux-x64/qt-$QT_VERSION/plugins/sqldrivers stage/qt/bin

	cp -rl $NX_VMS_DIR/build_environment/target/bin/$BUILD_CONFIGURATION/vms_gateway stage/vms_gateway/bin/vms_gateway
	cp -rl $NX_VMS_DIR/build_environment/target/lib/$BUILD_CONFIGURATION/* stage/vms_gateway/lib
}

main $@
