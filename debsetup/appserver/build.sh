#!/bin/bash

TARGET=/opt/networkoptix/entcontroller
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
ETCTARGET=$TARGET/etc
VARTARGET=$TARGET/var
SHARETARGET=$TARGET/share
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

PACKAGENAME=networkoptix-entcontroller
VERSION=`python ../../common/common_version.py`
ARCHITECTURE=amd64

STAGEBASE=package
STAGE=$STAGEBASE/${PACKAGENAME}_${VERSION}_${ARCHITECTURE}
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
ETCSTAGE=$STAGE$ETCTARGET
VARSTAGE=$STAGE$VARTARGET
SHARESTAGE=$STAGE$SHARETARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET

SERVER_BIN_PATH=../../appserver/setup/build/exe.linux-x86_64-2.7

# Prepare stage dir
sudo rm -rf $STAGEBASE
mkdir -p $BINSTAGE
mkdir -p $LIBSTAGE
mkdir -p $ETCSTAGE
mkdir -p $VARSTAGE
mkdir -p $SHARESTAGE
mkdir -p $INITSTAGE
mkdir -p $INITDSTAGE

# Copy entcontroller binary
cp -a $SERVER_BIN_PATH $STAGEBASE/tmp
mv $STAGEBASE/tmp/*.so $STAGEBASE/tmp/libpython2.7.so.1.0 $LIBSTAGE
mv $STAGEBASE/tmp/entcontroller $STAGEBASE/tmp/dbconfig  $STAGEBASE/tmp/library.zip $BINSTAGE
mv $STAGEBASE/tmp/db $VARSTAGE
mv $STAGEBASE/tmp/static $STAGEBASE/tmp $SHARESTAGE

# Copy upstart and sysv script
install -m 755 init/networkoptix-entcontroller.conf $INITSTAGE
install -m 755 init.d/networkoptix-entcontroller $INITDSTAGE

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" > $STAGE/DEBIAN/control
cp debian/postinst $STAGE/DEBIAN
cp debian/prerm $STAGE/DEBIAN
cp debian/templates $STAGE/DEBIAN

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums)

sudo chown -R root:root $STAGEBASE
# (cd $STAGEBASE; dpkg-deb -b networkoptix-entcontroller_${VERSION}_amd64)
