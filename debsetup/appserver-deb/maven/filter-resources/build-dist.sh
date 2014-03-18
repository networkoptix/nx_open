#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}

#. ../common.sh
PACKAGENAME=${COMPANY_NAME}-entcontroller
VERSION=${release.version}
ARCHITECTURE=${os.arch}

TARGET=/opt/${COMPANY_NAME}/entcontroller
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
ETCTARGET=$TARGET/etc
VARTARGET=$TARGET/var
SHARETARGET=$TARGET/share
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

STAGEBASE=deb
STAGE=$STAGEBASE/${PACKAGENAME}-${release.version}.${buildNumber}-${arch}-${build.configuration}-beta
PKGSTAGE=$STAGE$TARGET
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
ETCSTAGE=$STAGE$ETCTARGET
VARSTAGE=$STAGE$VARTARGET
SHARESTAGE=$STAGE$SHARETARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET

PROXY_BIN_PATH=${libdir}/bin/${build.configuration}
PROXY_LIB_PATH=${libdir}/lib/${build.configuration}
ECS_PRESTAGE_PATH=${libdir}/../../appserver/setup/build/stage
SCRIPTS_PATH=${basedir}/../scripts
	
#. $SERVER_BIN_PATH/env.sh

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE
mkdir -p $LIBSTAGE
mkdir -p $ETCSTAGE
mkdir -p $INITSTAGE
mkdir -p $INITDSTAGE

chmod -R 755 $STAGEBASE

############### Enterprise Controller
cp -r $ECS_PRESTAGE_PATH/* $PKGSTAGE

# Fix permissions after Boris
find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
find $PKGSTAGE -type f -print0 | xargs -0 chmod 644
chmod -R 755 $BINSTAGE
chmod 644 $BINSTAGE/library.zip

# Copy upstart and sysv scripts
install -m 644 init/networkoptix-entcontroller.conf $INITSTAGE/${COMPANY_NAME}-entcontroller.conf
install -m 755 init.d/networkoptix-entcontroller $INITDSTAGE/${COMPANY_NAME}-entcontroller


################ Media Proxy

# Copy mediaproxy binary
install -m 755 $PROXY_BIN_PATH/mediaproxy $BINSTAGE/mediaproxy-bin

# Copy libraries
cp -P $PROXY_LIB_PATH/*.so* $LIBSTAGE

# Strip and remove rpath
if [ '${build.configuration}' == 'release' ]
then
  for f in `find $LIBSTAGE -type f`
  do
    strip $f
    chrpath -d $f
  done
fi

find $SHARESTAGE -name \*.pyc -delete

# Copy mediaproxy startup script
install -m 755 bin/mediaproxy $BINSTAGE
install -m 755 $SCRIPTS_PATH/config_helper.py $BINSTAGE

install -m 644 init/networkoptix-mediaproxy.conf $INITSTAGE/${COMPANY_NAME}-mediaproxy.conf
install -m 755 init.d/networkoptix-mediaproxy $INITDSTAGE/${COMPANY_NAME}-mediaproxy

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control
install -m 755 debian/postinst $STAGE/DEBIAN
install -m 755 debian/prerm $STAGE/DEBIAN
install -m 644 debian/templates $STAGE/DEBIAN
install -m 644 debian/conffiles $STAGE/DEBIAN

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums; chmod 644 DEBIAN/md5sums)

(cd $STAGEBASE; fakeroot dpkg-deb -b ${PACKAGENAME}-${release.version}.${buildNumber}-${arch}-${build.configuration}-beta)
