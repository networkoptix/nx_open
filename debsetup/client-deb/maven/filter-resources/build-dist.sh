#!/bin/bash

set -e

COMPANY_NAME=@deb.customization.company.name@
FULL_COMPANY_NAME="@company.name@"
FULL_PRODUCT_NAME="@company.name@ @product.name@ Client.conf"
FULL_APPLAUNCHER_NAME="@company.name@ Launcher.conf"

VERSION=@release.version@
FULLVERSION=@release.version@.@buildNumber@
MINORVERSION=@parsedVersion.majorVersion@.@parsedVersion.minorVersion@
ARCHITECTURE=@os.arch@

TARGET=/opt/$COMPANY_NAME/client/$FULLVERSION
USRTARGET=/usr
BINTARGET=$TARGET/bin
BGTARGET=$TARGET/share/pictures/sample-backgrounds
HELPTARGET=$TARGET/help
ICONTARGET=$USRTARGET/share/icons
LIBTARGET=$TARGET/lib
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

FINALNAME=@artifact.name.client@
UPDATE_NAME=@artifact.name.client_update@.zip

STAGEBASE=deb
STAGE=$STAGEBASE/$FINALNAME
STAGETARGET=$STAGE/$TARGET
BINSTAGE=$STAGE$BINTARGET
BGSTAGE=$STAGE$BGTARGET
HELPSTAGE=$STAGE$HELPTARGET
ICONSTAGE=$STAGE$ICONTARGET
LIBSTAGE=$STAGE$LIBTARGET

CLIENT_BIN_PATH=@libdir@/bin/@build.configuration@
CLIENT_IMAGEFORMATS_PATH=$CLIENT_BIN_PATH/imageformats
CLIENT_MEDIASERVICE_PATH=$CLIENT_BIN_PATH/mediaservice
CLIENT_AUDIO_PATH=$CLIENT_BIN_PATH/audio
CLIENT_XCBGLINTEGRATIONS_PATH=$CLIENT_BIN_PATH/xcbglintegrations
CLIENT_PLATFORMINPUTCONTEXTS_PATH=$CLIENT_BIN_PATH/platforminputcontexts
CLIENT_QML_PATH=$CLIENT_BIN_PATH/qml
CLIENT_VOX_PATH=$CLIENT_BIN_PATH/vox
CLIENT_PLATFORMS_PATH=$CLIENT_BIN_PATH/platforms
CLIENT_BG_PATH=@libdir@/backgrounds
CLIENT_HELP_PATH=@ClientHelpSourceDir@
ICONS_PATH=@customization.dir@/icons/linux/hicolor
CLIENT_LIB_PATH=@libdir@/lib/@build.configuration@
BUILD_INFO_TXT=@libdir@/build_info.txt

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE/imageformats
mkdir -p $BINSTAGE/mediaservice
mkdir -p $BINSTAGE/audio
mkdir -p $BINSTAGE/platforminputcontexts
mkdir -p $HELPSTAGE
mkdir -p $LIBSTAGE
mkdir -p $BGSTAGE
mkdir -p $ICONSTAGE
mkdir -p "$STAGE/etc/xdg/$FULL_COMPANY_NAME"
mv -f debian/client.conf $STAGE/etc/xdg/"$FULL_COMPANY_NAME"/"$FULL_PRODUCT_NAME"
mv -f debian/applauncher.conf $STAGE/etc/xdg/"$FULL_COMPANY_NAME"/"$FULL_APPLAUNCHER_NAME"
mv -f usr/share/applications/icon.desktop usr/share/applications/@installer.name@.desktop
mv -f usr/share/applications/protocol.desktop usr/share/applications/@uri.protocol@.desktop

# Copy build_info.txt
cp -r $BUILD_INFO_TXT $BINSTAGE/../

cp -r qt.conf $BINSTAGE/

# Copy client binary, old version libs
cp -r $CLIENT_BIN_PATH/desktop_client $BINSTAGE/client-bin
cp -r $CLIENT_BIN_PATH/applauncher $BINSTAGE/applauncher-bin
cp -r bin/client $BINSTAGE
cp -r $CLIENT_BIN_PATH/@launcher.version.file@ $BINSTAGE
cp -r bin/applauncher $BINSTAGE

# Copy icons
cp -P -Rf usr $STAGE
cp -P -Rf $ICONS_PATH $ICONSTAGE
for f in `find $ICONSTAGE -name "*.png"`; do mv $f `dirname $f`/`basename $f .png`-@customization@.png; done

# Copy help
cp -r $CLIENT_HELP_PATH/* $HELPSTAGE

# Copy fonts
cp -r "$CLIENT_BIN_PATH/fonts" "$BINSTAGE"

# Copy backgrounds
cp -r $CLIENT_BG_PATH/* $BGSTAGE

# Copy libraries, imageformats, mediaservice
cp -r $CLIENT_LIB_PATH/*.so* $LIBSTAGE
cp -r $CLIENT_PLATFORMINPUTCONTEXTS_PATH/*.* $BINSTAGE/platforminputcontexts
cp -r $CLIENT_IMAGEFORMATS_PATH/*.* $BINSTAGE/imageformats
cp -r $CLIENT_MEDIASERVICE_PATH/*.* $BINSTAGE/mediaservice
cp -r $CLIENT_XCBGLINTEGRATIONS_PATH $BINSTAGE
cp -r $CLIENT_QML_PATH $BINSTAGE
if [ '@arch@' != 'arm' ]; then cp -r $CLIENT_AUDIO_PATH/*.* $BINSTAGE/audio; fi
cp -r $CLIENT_VOX_PATH $BINSTAGE
cp -r $CLIENT_PLATFORMS_PATH $BINSTAGE
rm -f $LIBSTAGE/*.debug

# Copy Qt libs
QTLIBS="Core Gui Widgets WebKit WebChannel WebKitWidgets OpenGL Multimedia MultimediaQuick_p Qml Quick QuickWidgets LabsTemplates X11Extras XcbQpa DBus Xml XmlPatterns Concurrent Network Sql PrintSupport"
if [ '@arch@' == 'arm' ]
then
  QTLIBS+=( Sensors )
fi

for var in $QTLIBS
do
    qtlib=libQt5$var.so
    echo "Adding Qt lib" $qtlib
    cp -P @qt.dir@/lib/$qtlib* $LIBSTAGE
done

if [ '@arch@' != 'arm' ]
then
    cp -r /usr/lib/@arch.dir@/libXss.so.1* $LIBSTAGE
    cp -r /lib/@arch.dir@/libpng12.so* $LIBSTAGE
    cp -r /usr/lib/@arch.dir@/libopenal.so.1* $LIBSTAGE
    cp -P @qt.dir@/lib/libicu*.so* $LIBSTAGE
fi

find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
find $PKGSTAGE -type f -print0 | xargs -0 chmod 644

chmod 755 $BINSTAGE/*

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN
chmod g-s,go+rx $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control

for f in $(ls debian); do
    if [ $f != 'control.template' ]; then install -m 755 debian/$f $STAGE/DEBIAN; fi
done

(cd $STAGE; find * -type f -not -regex '^DEBIAN/.*' -print0 | xargs -0 md5sum > DEBIAN/md5sums; chmod 644 DEBIAN/md5sums)

(cd $STAGEBASE; fakeroot dpkg-deb -b $FINALNAME)

mkdir -p "$STAGETARGET/share/icons"
cp "$ICONSTAGE/hicolor/scalable/apps"/* "$STAGETARGET/share/icons"
cp "bin/update.json" "$STAGETARGET"
echo "client.finalName=$FINALNAME" >> finalname-client.properties
echo "zip -y -r $UPDATE_NAME $STAGETARGET"
(cd "$STAGETARGET"; zip -y -r "$UPDATE_NAME" ./*)

if [ '@distribution_output_dir@' != '' ]; then
    mv "$STAGETARGET/$UPDATE_NAME" "@distribution_output_dir@/"
    mv "$STAGEBASE/$FINALNAME.deb" "@distribution_output_dir@/"
else
    mv "$STAGETARGET/$UPDATE_NAME" "@project.build.directory@/"
fi
