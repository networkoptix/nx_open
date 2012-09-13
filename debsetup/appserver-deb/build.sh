#!/bin/bash

. ../common.sh

TARGET=/opt/networkoptix/entcontroller
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
ETCTARGET=$TARGET/etc
VARTARGET=$TARGET/var
SHARETARGET=$TARGET/share
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

PACKAGENAME=networkoptix-entcontroller
STAGEBASE=package
STAGE=$STAGEBASE/${PACKAGENAME}_${VERSION}_${ARCHITECTURE}
PKGSTAGE=$STAGE$TARGET
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
ETCSTAGE=$STAGE$ETCTARGET
VARSTAGE=$STAGE$VARTARGET
SHARESTAGE=$STAGE$SHARETARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET

PROXY_BIN_PATH=../../mediaproxy/bin/release
ECS_PRESTAGE_PATH=../../appserver/setup/build/stage

function build_ecs {
    pushd ../../appserver/setup
    python setup.py build
    popd
}

build_ecs

# Prepare stage dir
sudo rm -rf $STAGEBASE
mkdir -p $PKGSTAGE
rmdir $PKGSTAGE

############### Enterprise Controller
cp -r $ECS_PRESTAGE_PATH $PKGSTAGE
cp /lib/$ARCH-linux-gnu/libssl.so.1.0.0 $LIBSTAGE
cp /lib/$ARCH-linux-gnu/libcrypto.so.1.0.0 $LIBSTAGE

mkdir -p $ETCSTAGE
touch $ETCSTAGE/entcontroller.conf

mkdir -p $INITSTAGE
mkdir -p $INITDSTAGE

# Copy upstart and sysv scripts
install -m 644 init/networkoptix-entcontroller.conf $INITSTAGE
install -m 644 init.d/networkoptix-entcontroller $INITDSTAGE


################ Media Proxy

QT_FILES="libQtXml.so.4 libQtGui.so.4 libQtNetwork.so.4 libQtCore.so.4"

# Copy mediaproxy binary
install -m 755 $PROXY_BIN_PATH/mediaproxy $BINSTAGE/mediaproxy-bin

# Copy mediaproxy startup script
install -m 755 bin/mediaproxy $BINSTAGE

# Copy required qt libraries
for file in $QT_FILES
do
    install -m 644  $QT_LIB_PATH/$file $LIBSTAGE
done

# Mediaproxy upstart and sysv scripts
install -m 644 init/networkoptix-mediaproxy.conf $INITSTAGE
install -m 644 init.d/networkoptix-mediaproxy $INITDSTAGE

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control
cp debian/postinst $STAGE/DEBIAN
cp debian/prerm $STAGE/DEBIAN
cp debian/templates $STAGE/DEBIAN
cp debian/conffiles $STAGE/DEBIAN

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums)

sudo chown -R root:root $STAGEBASE
(cd $STAGEBASE; sudo dpkg-deb -b networkoptix-entcontroller_${VERSION}_${ARCHITECTURE})
