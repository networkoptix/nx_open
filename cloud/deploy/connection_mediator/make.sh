#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=connection_mediator

function stage()
{
    rm -rf stage
    check_vms_dirs

	mkdir -p stage/connection_mediator/bin stage/connection_mediator/lib stage/qt/lib stage/qt/bin
	cp -rl $environment/packages/linux-x64/qt-$QT_VERSION/lib/* stage/qt/lib
	cp -rl $environment/packages/linux-x64/qt-$QT_VERSION/plugins/sqldrivers stage/qt/bin

	cp -rl $NX_VMS_DIR/build_environment/target/bin/$BUILD_CONFIGURATION/connection_mediator stage/connection_mediator/bin/connection_mediator
	cp -rl $NX_VMS_DIR/build_environment/target/lib/$BUILD_CONFIGURATION/* stage/connection_mediator/lib
}

main $@
