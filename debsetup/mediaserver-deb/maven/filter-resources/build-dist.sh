#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}

PACKAGENAME=$COMPANY_NAME-mediaserver
VERSION=${release.version}
ARCHITECTURE=${os.arch}

TARGET=/opt/$COMPANY_NAME/mediaserver
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
LIBPLUGINTARGET=$BINTARGET/plugins
ETCTARGET=$TARGET/etc
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

STAGEBASE=deb
STAGE=$STAGEBASE/${PACKAGENAME}-${release.version}.${buildNumber}-${arch}-${build.configuration}-beta
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
LIBPLUGINSTAGE=$STAGE$LIBPLUGINTARGET
ETCSTAGE=$STAGE$ETCTARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET

SERVER_BIN_PATH=${libdir}/bin/${build.configuration}
#SERVER_SQLDRIVERS_PATH=$SERVER_BIN_PATH/sqldrivers
SERVER_IMAGEFORMATS_PATH=$SERVER_BIN_PATH/imageformats
SERVER_LIB_PATH=${libdir}/lib/${build.configuration}
SERVER_LIB_PLUGIN_PATH=$SERVER_BIN_PATH/plugins
SCRIPTS_PATH=${basedir}/../scripts

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE
mkdir -p $BINSTAGE/imageformats
mkdir -p $LIBSTAGE
mkdir -p $LIBPLUGINSTAGE
mkdir -p $ETCSTAGE
mkdir -p $INITSTAGE
mkdir -p $INITDSTAGE

# Copy libraries
cp -P $SERVER_LIB_PATH/*.so* $LIBSTAGE
cp -r $SERVER_IMAGEFORMATS_PATH/*.* $BINSTAGE/imageformats
cp -P $SERVER_LIB_PLUGIN_PATH/*.so* $LIBPLUGINSTAGE
#cp -r $SERVER_SQLDRIVERS_PATH $BINSTAGE

# Strip and remove rpath

if [ '${build.configuration}' == 'release' ]
then
  for f in `find $LIBPLUGINSTAGE -type f`
  do
    strip $f
  done

  for f in `find $LIBSTAGE -type f`
  do
    strip $f
  done
fi

find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
find $PKGSTAGE -type f -print0 | xargs -0 chmod 644
chmod -R 755 $BINSTAGE

# Copy mediaserver binary and sqldrivers
install -m 755 $SERVER_BIN_PATH/mediaserver $BINSTAGE/mediaserver-bin
install -m 755 $SCRIPTS_PATH/config_helper.py $BINSTAGE

# Copy mediaserver startup script
install -m 755 bin/mediaserver $BINSTAGE

# Copy upstart and sysv script
install -m 644 init/networkoptix-mediaserver.conf $INITSTAGE/$COMPANY_NAME-mediaserver.conf
install -m 755 init.d/networkoptix-mediaserver $INITDSTAGE/$COMPANY_NAME-mediaserver

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control
install -m 755 debian/postinst $STAGE/DEBIAN
install -m 755 debian/prerm $STAGE/DEBIAN
install -m 644 debian/templates $STAGE/DEBIAN

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums; chmod 644 DEBIAN/md5sums)

(cd $STAGEBASE; fakeroot dpkg-deb -b ${PACKAGENAME}-${release.version}.${buildNumber}-${arch}-${build.configuration}-beta)
cp -P $SERVER_LIB_PATH/*.debug ${project.build.directory}
cp -P $SERVER_BIN_PATH/*.debug ${project.build.directory}
cp -P $SERVER_LIB_PLUGIN_PATH/*.debug ${project.build.directory}
tar czf ./${PACKAGENAME}-${release.version}.${buildNumber}-${arch}-${build.configuration}-debug-symbols.tar.gz ./*.debug