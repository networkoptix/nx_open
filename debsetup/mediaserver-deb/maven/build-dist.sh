#!/bin/bash

#. ../common.sh
PACKAGENAME=networkoptix-mediaserver
VERSION=${project.version}
ARCHITECTURE=${os.arch}

TARGET=/opt/networkoptix/mediaserver
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
ETCTARGET=$TARGET/etc
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

STAGEBASE=deb
STAGE=$STAGEBASE/${PACKAGENAME}-${project.version}.${buildNumber}-${arch}-${build.configuration}-${customization}
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
ETCSTAGE=$STAGE$ETCTARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET

SERVER_BIN_PATH=${libdir}/bin/${build.configuration}
SERVER_LIB_PATH=${libdir}/build/bin/${build.configuration}
	
. $SERVER_BIN_PATH/env.sh

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE
mkdir -p $LIBSTAGE
mkdir -p $ETCSTAGE
mkdir -p $INITSTAGE
mkdir -p $INITDSTAGE

# Copy mediaserver binary
install -m 755 $SERVER_BIN_PATH/mediaserver* $BINSTAGE

# Copy mediaserver startup script
install -m 755 bin/mediaserver $BINSTAGE

# Copy upstart and sysv script
install -m 755 init/networkoptix-mediaserver.conf $INITSTAGE
install -m 755 init.d/networkoptix-mediaserver $INITDSTAGE

# Copy libraries
cp -P $SERVER_LIB_PATH/*.so* $LIBSTAGE

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control
cp debian/postinst $STAGE/DEBIAN
cp debian/prerm $STAGE/DEBIAN
cp debian/templates $STAGE/DEBIAN

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums)

sudo chown -R root:root $STAGEBASE

(cd $STAGEBASE; sudo dpkg-deb -b ${PACKAGENAME}-${project.version}.${buildNumber}-${arch}-${build.configuration}-${customization})
