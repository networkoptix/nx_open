#!/bin/bash

#. ../common.sh
PACKAGENAME=networkoptix-entcontroller
VERSION=${project.version}
ARCHITECTURE=${os.arch}

TARGET=/opt/networkoptix/entcontroller
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
ETCTARGET=$TARGET/etc
VARTARGET=$TARGET/var
SHARETARGET=$TARGET/share
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

STAGEBASE=deb
STAGE=$STAGEBASE/${PACKAGENAME}-${project.version}.${buildNumber}-${arch}-${build.configuration}
PKGSTAGE=$STAGE$TARGET
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
ETCSTAGE=$STAGE$ETCTARGET
VARSTAGE=$STAGE$VARTARGET
SHARESTAGE=$STAGE$SHARETARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET

PROXY_BIN_PATH=${project.build.directory}/bin
PROXY_LIB_PATH=${project.build.directory}/build/bin/${build.configuration}
ECS_PRESTAGE_PATH=${project.build.directory}/appserver/*
	
#. $SERVER_BIN_PATH/env.sh

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE
mkdir -p $LIBSTAGE
mkdir -p $ETCSTAGE
mkdir -p $INITSTAGE
mkdir -p $INITDSTAGE

############### Enterprise Controller
cp -r $ECS_PRESTAGE_PATH $PKGSTAGE
cp ${qt.dir}/libssl.so.1.0.0 $LIBSTAGE
cp ${qt.dir}/libcrypto.so.1.0.0 $LIBSTAGE

touch $ETCSTAGE/entcontroller.conf


# Copy upstart and sysv scripts
install -m 755 init/networkoptix-entcontroller.conf $INITSTAGE
install -m 755 init.d/networkoptix-entcontroller $INITDSTAGE


################ Media Proxy

# Copy mediaproxy binary
install -m 755 $PROXY_BIN_PATH/mediaproxy-bin $BINSTAGE

# Copy libraries
cp -P $PROXY_LIB_PATH/*.so* $LIBSTAGE
cp -P ${qt.dir}/libaudio.so* $LIBSTAGE
cp -P ${qt.dir}/libXi.so* $LIBSTAGE
cp -P ${qt.dir}/libXt.so* $LIBSTAGE
cp -P ${qt.dir}/libXrender.so* $LIBSTAGE
cp -P ${qt.dir}/libfontconfig.so* $LIBSTAGE
cp -P ${qt.dir}/libICE.so* $LIBSTAGE
cp -P ${qt.dir}/libSM.so* $LIBSTAGE

# Copy mediaproxy startup script
install -m 755 bin/mediaproxy $BINSTAGE
install -m 755 init/networkoptix-mediaproxy.conf $INITSTAGE
install -m 755 init.d/networkoptix-mediaproxy $INITDSTAGE

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
(cd $STAGEBASE; sudo dpkg-deb -b ${PACKAGENAME}-${project.version}.${buildNumber}-${arch}-${build.configuration})
