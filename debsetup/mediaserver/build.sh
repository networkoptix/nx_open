#!/bin/bash

TARGET=/opt/networkoptix/mediaserver
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
ETCTARGET=$TARGET/etc
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

PACKAGENAME=networkoptix-mediaserver
VERSION=`python ../../common/common_version.py`
ARCH=`uname -i`

QT_PATH=$(dirname $(dirname $(which qmake)))
case $ARCH in
    "i386")
        QT_LIB_PATH="$QT_PATH/lib/i386-linux-gnu"
        ARCHITECTURE=i386
        ;;
    "x86_64")
        QT_LIB_PATH="$QT_PATH/lib"
        ARCHITECTURE=amd64
        ;;
esac

STAGEBASE=package
STAGE=$STAGEBASE/${PACKAGENAME}_${VERSION}_${ARCHITECTURE}
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
ETCSTAGE=$STAGE$ETCTARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET

SERVER_BIN_PATH=../../mediaserver/bin/release
. $SERVER_BIN_PATH/env.sh

QT_FILES="libQtXml.so.4 libQtGui.so.4 libQtNetwork.so.4 libQtCore.so.4"
FFMPEG_FILES="libavcodec.so.54 libavdevice.so.53 libavfilter.so.2 libavformat.so.54 libavutil.so.51 libswscale.so.2"


# Prepare stage dir
sudo rm -rf $STAGEBASE
mkdir -p $BINSTAGE
mkdir -p $LIBSTAGE
mkdir -p $ETCSTAGE
mkdir -p $INITSTAGE
mkdir -p $INITDSTAGE

# Copy mediaserver binary
install -m 755 $SERVER_BIN_PATH/mediaserver $BINSTAGE/mediaserver-bin

# Copy mediaserver startup script
install -m 755 bin/mediaserver $BINSTAGE

# Copy upstart and sysv script
install -m 755 init/networkoptix-mediaserver.conf $INITSTAGE
install -m 755 init.d/networkoptix-mediaserver $INITDSTAGE

# Copy required qt libraries
for file in $QT_FILES
do
    install -m 644  $QT_LIB_PATH/$file $LIBSTAGE
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

# Copy libcrypto
install -m 644 /lib/$ARCH-linux-gnu/libcrypto.so.1.0.0 $LIBSTAGE

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control
cp debian/postinst $STAGE/DEBIAN
cp debian/prerm $STAGE/DEBIAN
cp debian/templates $STAGE/DEBIAN

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums)

sudo chown -R root:root $STAGEBASE

(cd $STAGEBASE; sudo dpkg-deb -b networkoptix-mediaserver_${VERSION}_${ARCHITECTURE})
