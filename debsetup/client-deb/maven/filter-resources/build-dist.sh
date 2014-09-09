#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}
FULL_COMPANY_NAME="${company.name}"
FULL_PRODUCT_NAME="${company.name} ${product.name} Client.conf"
FULL_APPLAUNCHER_NAME="${company.name} Launcher.conf"

PACKAGENAME=$COMPANY_NAME-client
VERSION=${release.version}
MINORVERSION=${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
ARCHITECTURE=${os.arch}

TARGET=/opt/$COMPANY_NAME/client/$MINORVERSION
USRTARGET=/usr
BINTARGET=$TARGET/bin
BGTARGET=$TARGET/share/pictures/sample-backgrounds
HELPTARGET=$TARGET/help
ICONTARGET=$USRTARGET/share/icons
LIBTARGET=$TARGET/lib
INITTARGET=/etc/init
INITDTARGET=/etc/init.d
BETA=""
if [[ "$beta" == "true" ]]; then 
  $BETA="-beta" 
fi 

FINALNAME=${PACKAGENAME}-$VERSION.${buildNumber}-${arch}-${build.configuration}$BETA

STAGEBASE=deb
STAGE=$STAGEBASE/$FINALNAME
BINSTAGE=$STAGE$BINTARGET
BGSTAGE=$STAGE$BGTARGET
HELPSTAGE=$STAGE$HELPTARGET
ICONSTAGE=$STAGE$ICONTARGET
LIBSTAGE=$STAGE$LIBTARGET

CLIENT_BIN_PATH=${libdir}/bin/${build.configuration}
CLIENT_STYLES_PATH=$CLIENT_BIN_PATH/styles
CLIENT_IMAGEFORMATS_PATH=$CLIENT_BIN_PATH/imageformats
CLIENT_VOX_PATH=$CLIENT_BIN_PATH/vox
CLIENT_PLATFORMS_PATH=$CLIENT_BIN_PATH/platforms
CLIENT_BG_PATH=${libdir}/backgrounds
CLIENT_HELP_PATH=${environment.dir}/help/${release.version}/${customization}
ICONS_PATH=${customization.dir}/icons/hicolor
CLIENT_LIB_PATH=${libdir}/lib/${build.configuration}

#. $CLIENT_BIN_PATH/env.sh

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE/styles
mkdir -p $BINSTAGE/imageformats
mkdir -p $HELPSTAGE
mkdir -p $LIBSTAGE
mkdir -p $BGSTAGE
mkdir -p $ICONSTAGE
mkdir -p "$STAGE/etc/xdg/$FULL_COMPANY_NAME"
mv -f debian/client.conf $STAGE/etc/xdg/"$FULL_COMPANY_NAME"/"$FULL_PRODUCT_NAME"
mv -f debian/applauncher.conf $STAGE/etc/xdg/"$FULL_COMPANY_NAME"/"$FULL_APPLAUNCHER_NAME"
mv -f usr/share/applications/icon.desktop usr/share/applications/${namespace.additional}.desktop

# Copy client binary, old version libs
cp -r $CLIENT_BIN_PATH/client $BINSTAGE/client-bin
cp -r $CLIENT_BIN_PATH/applauncher $BINSTAGE/applauncher-bin
cp -r bin/applauncher $BINSTAGE

# Copy icons
cp -P -Rf usr $STAGE
cp -P -Rf $ICONS_PATH $ICONSTAGE
for f in `find $ICONSTAGE -name *.png`; do mv $f `dirname $f`/`basename $f .png`-${customization}.png; done

# Copy help
cp -r $CLIENT_HELP_PATH/** $HELPSTAGE

# Copy backgrounds
cp -r $CLIENT_BG_PATH/* $BGSTAGE

# Copy libraries, styles, imageformats
cp -r $CLIENT_LIB_PATH/*.so* $LIBSTAGE
cp -r $CLIENT_STYLES_PATH/*.* $BINSTAGE/styles
cp -r $CLIENT_IMAGEFORMATS_PATH/*.* $BINSTAGE/imageformats
cp -r $CLIENT_VOX_PATH $BINSTAGE
cp -r $CLIENT_PLATFORMS_PATH $BINSTAGE

find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
find $PKGSTAGE -type f -print0 | xargs -0 chmod 644

chmod 755 $BINSTAGE/*

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control

install -m 755 debian/prerm $STAGE/DEBIAN
install -m 755 debian/postinst $STAGE/DEBIAN

(cd $STAGE; find * -type f -not -regex '^DEBIAN/.*' -print0 | xargs -0 md5sum > DEBIAN/md5sums; chmod 644 DEBIAN/md5sums)

(cd $STAGEBASE; fakeroot dpkg-deb -b ${PACKAGENAME}-$FINALNAME)
echo "$FINALNAME" >> finalname-client.properties