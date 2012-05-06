#!/bin/bash

#. ../common.sh

TARGET=/opt/networkoptix/${project.artifactId}
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
ETCTARGET=$TARGET/etc
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

PACKAGENAME=networkoptix-${project.artifactId}
VERSION=${project.version}
ARCHITECTURE=${os.arch}

STAGEBASE=deb
STAGE=$STAGEBASE/${PACKAGENAME}-${project.version}-${arch}-${build.configuration}
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
ETCSTAGE=$STAGE$ETCTARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET

SERVER_BIN_PATH=${project.build.directory}/bin/${build.configuration}
SERVER_LIB_PATH=${project.build.directory}/build/bin/${build.configuration}
	
#SERVER_BIN_PATH=../../mediaserver/x64/bin/release
#SERVER_LIB_PATH=../../mediaserver/x64/build/bin/release

. $SERVER_BIN_PATH/env.sh

#QT_FILES="libQtXml.so.4 libQtGui.so.4 libQtNetwork.so.4 libQtCore.so.4"
#FFMPEG_FILES="libavcodec.so.[0-9][0-9] libavdevice.so.[0-9][0-9] libavfilter.so.2 libavformat.so.[0-9][0-9] libavutil.so.51 libswscale.so.2"

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
# Copy required qt libraries
#for file in $QT_FILES
#do
#    install -m 644  $QT_LIB_PATH/$file $LIBSTAGE
#done

# Copy required ffmpeg libraries
#for file in $FFMPEG_FILES
#do
#    install -m 644 $FFMPEG_PATH/lib/$file $LIBSTAGE
#done

# Copy qjson library
#install -m 644 $QJSON_PATH/libqjson.so.0 $LIBSTAGE

# Copy system libraries which may be absent on target platform
#install -m 644 /usr/lib/libprotobuf.so.7 $LIBSTAGE

# Copy libcrypto
#install -m 644 /lib/$ARCH-linux-gnu/libcrypto.so.1.0.0 $LIBSTAGE

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control
cp debian/postinst $STAGE/DEBIAN
cp debian/prerm $STAGE/DEBIAN
cp debian/templates $STAGE/DEBIAN

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums)

sudo chown -R root:root $STAGEBASE

(cd $STAGEBASE; sudo dpkg-deb -b ${PACKAGENAME}-${project.version}-${arch}-${build.configuration})
