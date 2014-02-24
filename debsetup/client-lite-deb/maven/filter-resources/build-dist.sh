#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
FULL_COMPANY_NAME="${company.name}"
FULL_PRODUCT_NAME="${company.name} ${product.name} Client.conf"

PACKAGENAME=$COMPANY_NAME-client-lite
VERSION=${release.version}
MINORVERSION=${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
ARCHITECTURE=${os.arch}

TARGET=/opt/$COMPANY_NAME/client-lite
USRTARGET=/usr
BINTARGET=$TARGET/bin
BGTARGET=$TARGET/share/pictures/sample-backgrounds
ICONTARGET=$USRTARGET/icons
LIBTARGET=$TARGET/lib
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

STAGEBASE=deb
STAGE=$STAGEBASE/${PACKAGENAME}-$VERSION.${buildNumber}-${arch}-${build.configuration}-beta
BINSTAGE=$STAGE$BINTARGET
BGSTAGE=$STAGE$BGTARGET
ICONSTAGE=$STAGE$ICONTARGET
LIBSTAGE=$STAGE$LIBTARGET

CLIENT_BIN_PATH=${libdir}/bin/${build.configuration}
CLIENT_STYLES_PATH=$CLIENT_BIN_PATH/styles
CLIENT_IMAGEFORMATS_PATH=$CLIENT_BIN_PATH/imageformats
CLIENT_VOX_PATH=${environment.dir}/festival.vox/lib
CLIENT_PLATFORMS_PATH=$CLIENT_BIN_PATH/platforms
CLIENT_BG_PATH=${libdir}/backgrounds
CLIENT_HELP_PATH=${environment.dir}/help/${release.version}/${customization}
ICONS_PATH=${customization.dir}/icons/hicolor
CLIENT_LIB_PATH=${libdir}/lib/${build.configuration}

#. $CLIENT_BIN_PATH/env.sh

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE/$MINORVERSION/styles
mkdir -p $BINSTAGE/$MINORVERSION/imageformats
mkdir -p $BINSTAGE/help
mkdir -p $LIBSTAGE
mkdir -p $BGSTAGE
mkdir -p $ICONSTAGE
mkdir -p "$STAGE/etc/xdg/${company.name}"
mv -f debian/client.conf $STAGE/etc/xdg/"$FULL_COMPANY_NAME"/"$FULL_PRODUCT_NAME"

# Copy client binary, x264, old version libs
cp -r $CLIENT_BIN_PATH/client $BINSTAGE/$MINORVERSION/client-bin
cp -r $CLIENT_BIN_PATH/applauncher $BINSTAGE/$MINORVERSION/applauncher-bin
cp -r bin/applauncher $BINSTAGE/$MINORVERSION
cp -r $CLIENT_BIN_PATH/x264 $BINSTAGE/$MINORVERSION

# Copy icons
cp -P -Rf usr $STAGE
cp -P -Rf $ICONS_PATH $ICONSTAGE
for f in `find $ICONSTAGE -name *.png`; do mv $f `dirname $f`/`basename $f .png`-${customization}.png; done

# Copy help
cp -r $CLIENT_HELP_PATH/** $BINSTAGE/help

# Copy backgrounds
cp -r $CLIENT_BG_PATH/* $BGSTAGE

# Copy libraries, styles, imageformats
cp -r $CLIENT_LIB_PATH/*.so* $LIBSTAGE
cp -r $CLIENT_STYLES_PATH/*.* $BINSTAGE/$MINORVERSION/styles
cp -r $CLIENT_IMAGEFORMATS_PATH/*.* $BINSTAGE/$MINORVERSION/imageformats
cp -r $CLIENT_VOX_PATH $BINSTAGE/$MINORVERSION
cp -r $CLIENT_PLATFORMS_PATH $BINSTAGE/$MINORVERSION

find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
find $PKGSTAGE -type f -print0 | xargs -0 chmod 644

chmod 755 $BINSTAGE/$MINORVERSION/*

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control

install -m 755 debian/prerm $STAGE/DEBIAN

(cd $STAGE; find * -type f -not -regex '^DEBIAN/.*' -print0 | xargs -0 md5sum > DEBIAN/md5sums; chmod 644 DEBIAN/md5sums)

(cd $STAGEBASE; fakeroot dpkg-deb -b ${PACKAGENAME}-$VERSION.${buildNumber}-${arch}-${build.configuration}-beta)
