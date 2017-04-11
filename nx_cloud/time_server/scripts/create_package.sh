#!/bin/bash

SRC_ROOT=$(readlink -e $(dirname "${BASH_SOURCE[0]}")/../../..)
TMP_DIR="ts`date +%N`"
PACKAGE_NAME=time_server.tar.gz

PACKAGES_ROOT=$SRC_ROOT/../buildenv/packages/linux-x64/
QT_VERSION=5.6.1
RELEASE=${1:-release}

mkdir ./$TMP_DIR

pushd $TMP_DIR
mkdir -p ./opt/networkoptix/time_server/bin
mkdir -p ./opt/networkoptix/time_server/lib
mkdir -p ./opt/networkoptix/time_server/var/log
mkdir -p ./opt/networkoptix/time_server/etc
mkdir -p ./etc/init.d/

cp -rf $SRC_ROOT/build_environment/target/lib/$RELEASE/* ./opt/networkoptix/time_server/lib/
cp -rf $SRC_ROOT/build_environment/target/bin/$RELEASE/time_server ./opt/networkoptix/time_server/bin/
cp -rf $PACKAGES_ROOT/qt-$QT_VERSION/lib/* ./opt/networkoptix/time_server/lib/
cp $SRC_ROOT/nx_cloud/time_server/scripts/nx-time-server.sh ./etc/init.d/nx-time-server

tar czf $PACKAGE_NAME ./opt ./etc
mv $PACKAGE_NAME ..
popd

rm -rf $TMP_DIR
