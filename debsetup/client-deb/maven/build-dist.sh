#!/bin/bash

#. ../common.sh
PACKAGENAME=networkoptix-client
VERSION=${project.version}
ARCHITECTURE=${os.arch}

TARGET=/opt/networkoptix/client
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
USRTARGET=/usr
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

STAGEBASE=deb
STAGE=$STAGEBASE/${PACKAGENAME}-${project.version}.${buildNumber}-${arch}-${build.configuration}-${customization}
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET

CLIENT_BIN_PATH=${libdir}/bin/${build.configuration}
CLIENT_STYLES_PATH=$CLIENT_BIN_PATH/styles
CLIENT_LIB_PATH=${libdir}/build/bin/${build.configuration}
	
. $CLIENT_BIN_PATH/env.sh

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE
mkdir -p $BINSTAGE/styles
mkdir -p $LIBSTAGE

# Copy client binary and x264
install -m 755 $CLIENT_BIN_PATH/client* $BINSTAGE
install -m 755 $CLIENT_BIN_PATH/x264 $BINSTAGE

# Copy client startup script
install -m 755 bin/client $BINSTAGE

# Copy icons
cp -P -Rf usr $STAGE

# Copy libraries
cp -P $CLIENT_LIB_PATH/*.so* $LIBSTAGE
cp -P $CLIENT_STYLES_PATH/*.* $BINSTAGE/styles

# Must use system libraries due to compatibility issues
# cp -P ${qt.dir}/libaudio.so* $LIBSTAGE
# cp -P ${qt.dir}/libXi.so* $LIBSTAGE
# cp -P ${qt.dir}/libXt.so* $LIBSTAGE
# cp -P ${qt.dir}/libXrender.so* $LIBSTAGE
# cp -P ${qt.dir}/libfontconfig.so* $LIBSTAGE
# cp -P ${qt.dir}/libICE.so* $LIBSTAGE
# cp -P ${qt.dir}/libSM.so* $LIBSTAGE

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums)

sudo chown -R root:root $STAGEBASE

(cd $STAGEBASE; sudo dpkg-deb -b ${PACKAGENAME}-${project.version}.${buildNumber}-${arch}-${build.configuration}-${customization})