#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=cloud_connect_test_util

function stage()
{
    rm -rf stage
    check_vms_dirs

	mkdir -p stage/cloud_connect_test_util/bin stage/cloud_connect_test_util/lib stage/qt/lib stage/qt/bin stage/var/log
	cp -rl $environment/packages/linux-x64/qt-$QT_VERSION/lib/* stage/qt/lib
	cp -rl $environment/packages/linux-x64/qt-$QT_VERSION/plugins/sqldrivers stage/qt/bin

	cp -rl $NX_VMS_DIR/build_environment/target/bin/$BUILD_CONFIGURATION/cloud_connect_test_util stage/cloud_connect_test_util/bin/cloud_connect_test_util
	cp -rl $NX_VMS_DIR/build_environment/target/lib/$BUILD_CONFIGURATION/* stage/cloud_connect_test_util/lib
}

main $@
