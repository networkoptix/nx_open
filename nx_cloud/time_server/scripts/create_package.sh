#!/bin/bash

SRC_ROOT=/home/ak/develop/netoptix_vms_3.0.0/
TMP_DIR="ts`date +%N`"
PACKAGE_NAME=time_server.tar.gz

PACKAGES_ROOT=$SRC_ROOT/../buildenv/packages/linux-x64/
QT_VERSION=5.6.1

mkdir ./$TMP_DIR

pushd $TMP_DIR
mkdir -p ./opt/networkoptix/time_server/bin
mkdir -p ./opt/networkoptix/time_server/lib
mkdir -p ./opt/networkoptix/time_server/var/log
mkdir -p ./opt/networkoptix/time_server/etc
mkdir -p ./etc/init.d/

cp -rf $SRC_ROOT/build_environment/target/lib/release/* ./opt/networkoptix/time_server/lib/
cp -rf $SRC_ROOT/build_environment/target/bin/release/time_server ./opt/networkoptix/time_server/bin/
cp -rf $PACKAGES_ROOT/qt-$QT_VERSION/lib/* ./opt/networkoptix/time_server/lib/
cp $SRC_ROOT/nx_cloud/time_server/scripts/nx-time-server ./etc/init.d/

tar czf $PACKAGE_NAME ./opt ./etc
mv $PACKAGE_NAME .. 
popd

rm -rf $TMP_DIR
