#!/bin/bash

set -e

COMPANY_NAME=@deb.customization.company.name@

VERSION=@release.version@
ARCHITECTURE=@os.arch@

TARGET=/opt/$COMPANY_NAME/mediaserver
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
LIBPLUGINTARGET=$BINTARGET/plugins
SHARETARGET=$TARGET/share
ETCTARGET=$TARGET/etc
INITTARGET=/etc/init
INITDTARGET=/etc/init.d
SYSTEMDTARGET=/etc/systemd/system

FINALNAME=@artifact.name.server@
UPDATE_NAME=@artifact.name.server_update@.zip

STAGEBASE=deb
STAGE=$STAGEBASE/$FINALNAME
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
LIBPLUGINSTAGE=$STAGE$LIBPLUGINTARGET
SHARESTAGE=$STAGE$SHARETARGET
ETCSTAGE=$STAGE$ETCTARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET
SYSTEMDSTAGE=$STAGE$SYSTEMDTARGET

SERVER_BIN_PATH=@libdir@/bin/@build.configuration@
SERVER_SHARE_PATH=@libdir@/share
#SERVER_SQLDRIVERS_PATH=$SERVER_BIN_PATH/sqldrivers
SERVER_DEB_PATH=@libdir@/deb
SERVER_VOX_PATH=$SERVER_BIN_PATH/vox
SERVER_LIB_PATH=@libdir@/lib/@build.configuration@
SERVER_LIB_PLUGIN_PATH=$SERVER_BIN_PATH/plugins
SCRIPTS_PATH=@basedir@/../scripts
BUILD_INFO_TXT=@libdir@/build_info.txt

# Prepare stage dir
rm -rf $STAGE
mkdir -p $BINSTAGE
mkdir -p $LIBSTAGE
mkdir -p $LIBPLUGINSTAGE
mkdir -p $ETCSTAGE
mkdir -p $SHARESTAGE
mkdir -p $INITSTAGE
mkdir -p $INITDSTAGE
mkdir -p $SYSTEMDSTAGE

# Copy build_info.txt
cp -r $BUILD_INFO_TXT $BINSTAGE/../

# Copy dbsync 2.2
if [ '@arch@' != 'arm' ]
then
    cp -r @packages.dir@/@rdep.target@/appserver-2.2.1/share/dbsync-2.2 $SHARESTAGE
    cp @libdir@/version.py $SHARESTAGE/dbsync-2.2/bin
fi

# Copy libraries
cp -P $SERVER_LIB_PATH/*.so* $LIBSTAGE
cp -P $SERVER_LIB_PLUGIN_PATH/*.so* $LIBPLUGINSTAGE
[ $COMPANY_NAME != "hanwha" ] && rm -rf $LIBPLUGINSTAGE/hanwha*
cp -r $SERVER_VOX_PATH $BINSTAGE
#'libstdc++.so.6 is needed on some machines
if [ '@arch@' != 'arm' ]
then
    cp -r /usr/lib/@arch.dir@/libstdc++.so.6* $LIBSTAGE
    cp -P @qt.dir@/lib/libicu*.so* $LIBSTAGE
fi

#copying qt libs
QTLIBS="Core Gui Xml XmlPatterns Concurrent Network Sql WebSockets"
for var in $QTLIBS
do
    qtlib=libQt5$var.so
    echo "Adding Qt lib" $qtlib
    cp -P @qt.dir@/lib/$qtlib* $LIBSTAGE
done

#cp -r $SERVER_SQLDRIVERS_PATH $BINSTAGE

# Strip and remove rpath

if [[ ('@build.configuration@' == 'release' || '@CMAKE_BUILD_TYPE@' == 'Release') \
    && '@arch@' != 'arm' ]]
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
if [ '@arch@' != 'arm' ]; then chmod 755 $SHARESTAGE/dbsync-2.2/bin/{dbsync,certgen}; fi

# Copy mediaserver binary and sqldrivers
install -m 755 $SERVER_BIN_PATH/mediaserver $BINSTAGE/mediaserver-bin
install -m 750 $SERVER_BIN_PATH/root_tool $BINSTAGE
install -m 755 $SERVER_BIN_PATH/testcamera $BINSTAGE
install -m 755 $SERVER_BIN_PATH/external.dat $BINSTAGE
install -m 755 $SCRIPTS_PATH/config_helper.py $BINSTAGE
install -m 755 $SCRIPTS_PATH/shell_utils.sh $BINSTAGE

# Copy mediaserver startup script
install -m 755 bin/mediaserver $BINSTAGE

# Copy upstart and sysv script
install -m 644 init/networkoptix-mediaserver.conf $INITSTAGE/$COMPANY_NAME-mediaserver.conf
install -m 755 init.d/networkoptix-mediaserver $INITDSTAGE/$COMPANY_NAME-mediaserver
install -m 644 systemd/networkoptix-mediaserver.service $SYSTEMDSTAGE/$COMPANY_NAME-mediaserver.service

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN
chmod 00775 $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control
install -m 755 debian/preinst $STAGE/DEBIAN
install -m 755 debian/postinst $STAGE/DEBIAN
install -m 755 debian/prerm $STAGE/DEBIAN
install -m 644 debian/templates $STAGE/DEBIAN

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums; chmod 644 DEBIAN/md5sums)

(cd $STAGEBASE; fakeroot dpkg-deb -b $FINALNAME)

cp -Rf $SERVER_DEB_PATH/* $STAGEBASE
(cd $STAGEBASE; zip -r ./$UPDATE_NAME ./ -x ./$FINALNAME/**\* $FINALNAME/)
mv $STAGEBASE/$UPDATE_NAME @project.build.directory@/
echo "server.finalName=$FINALNAME" >> finalname-server.properties
