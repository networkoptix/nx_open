#!/bin/bash

TARGET=/opt/networkoptix/mediaserver
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
ETCTARGET=$TARGET/etc
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

PACKAGENAME=networkoptix-mediaserver
VERSION=`python ../common/common_version.py`
ARCHITECTURE=amd64

STAGEBASE=package
STAGE=$STAGEBASE/${PACKAGENAME}_${VERSION}_${ARCHITECTURE}
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
ETCSTAGE=$STAGE$ETCTARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET

SERVER_BIN_PATH=../mediaserver/bin/release

. $SERVER_BIN_PATH/env.sh

QT_FILES="libQtXml.so.4 libQtGui.so.4 libQtNetwork.so.4 libQtCore.so.4"
FFMPEG_FILES="libavcodec.so.53 libavdevice.so.53 libavfilter.so.2 libavformat.so.53 libavutil.so.51 libpostproc.so.51 libswscale.so.2"

QT_PATH=$(dirname $(dirname $(which qmake)))

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE
mkdir -p $LIBSTAGE
mkdir -p $ETCSTAGE
mkdir -p $INITSTAGE
mkdir -p $INITDSTAGE

# Copy mediaserver binary
install -m 755 ../mediaserver/bin/release/mediaserver $BINSTAGE/mediaserver-bin

# Copy mediaserver startup script
install -m 755 bin/mediaserver $BINSTAGE

# Copy upstart and sysv script
install -m 755 init/networkoptix-mediaserver.conf $INITSTAGE
install -m 755 init.d/networkoptix-mediaserver $INITDSTAGE

# Copy required qt libraries
for file in $QT_FILES
do
    install -m 644  $QT_PATH/lib/$file $LIBSTAGE
done

# Copy required ffmpeg libraries
for file in $FFMPEG_FILES
do
    install -m 644 $FFMPEG_PATH/lib/$file $LIBSTAGE
done

# Copy qjson library
install -m 644 $QJSON_PATH/libqjson.so.0 $LIBSTAGE

# Copy system libraries which may be absent on target platform
install -m 644 /usr/lib/libprotobuf.so.7 $LIBSTAGE

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" > $STAGE/DEBIAN/control
cp debian/postinst $STAGE/DEBIAN
cp debian/prerm $STAGE/DEBIAN
cp debian/templates $STAGE/DEBIAN

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums)
# tar czvf ../mediaserver_orig.1.0.tar.gz *

(cd $STAGEBASE; dpkg-deb -b networkoptix-mediaserver_${VERSION}_amd64)
