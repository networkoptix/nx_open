#!/bin/bash

#. ../common.sh
PACKAGENAME=networkoptix-client
VERSION=${project.version}
ARCHITECTURE=${os.arch}

TARGET=/opt/networkoptix/client
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
ETCTARGET=$TARGET/etc
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

STAGEBASE=deb
STAGE=$STAGEBASE/${PACKAGENAME}-${project.version}.${buildNumber}-${arch}-${build.configuration}
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
ETCSTAGE=$STAGE$ETCTARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET

SERVER_BIN_PATH=${project.build.directory}/bin
SERVER_LIB_PATH=${project.build.directory}/build/bin/${build.configuration}
	
. $SERVER_BIN_PATH/env.sh

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE
mkdir -p $LIBSTAGE
mkdir -p $ETCSTAGE
mkdir -p $INITSTAGE
mkdir -p $INITDSTAGE

# Copy client binary
install -m 755 $SERVER_BIN_PATH/client* $BINSTAGE

# Copy client startup script
install -m 755 bin/client $BINSTAGE

# Copy libraries
cp -P $SERVER_LIB_PATH/*.so* $LIBSTAGE
cp -P ${qt.dir}/libaudio.so* $LIBSTAGE
cp -P ${qt.dir}/libXi.so* $LIBSTAGE
cp -P ${qt.dir}/libXt.so* $LIBSTAGE
cp -P ${qt.dir}/libXrender.so* $LIBSTAGE
cp -P ${qt.dir}/libfontconfig.so* $LIBSTAGE
cp -P ${qt.dir}/libICE.so* $LIBSTAGE
cp -P ${qt.dir}/libSM.so* $LIBSTAGE

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums)

sudo chown -R root:root $STAGEBASE

(cd $STAGEBASE; sudo dpkg-deb -b ${PACKAGENAME}-${project.version}.${buildNumber}-${arch}-${build.configuration})