#!/bin/bash

set -e

COMPANY_NAME=${deb.customization.company.name}
FULL_COMPANY_NAME="${company.name}"
FULL_PRODUCT_NAME="${company.name} ${product.name} Client.conf"
FULL_APPLAUNCHER_NAME="${company.name} Launcher.conf"

PACKAGENAME=${installer.name}-client
VERSION=${release.version}
FULLVERSION=${release.version}.${buildNumber}
MINORVERSION=${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
ARCHITECTURE=${os.arch}

TARGET=/opt/$COMPANY_NAME/client/$FULLVERSION
USRTARGET=/usr
BINTARGET=$TARGET/bin
BGTARGET=$TARGET/share/pictures/sample-backgrounds
HELPTARGET=$TARGET/help
ICONTARGET=$USRTARGET/share/icons
LIBTARGET=$TARGET/lib
INITTARGET=/etc/init
INITDTARGET=/etc/init.d
BETA=""
if [[ "${beta}" == "true" ]]; then
  BETA="-beta"
fi

FINALNAME=${PACKAGENAME}-$VERSION.${buildNumber}-${arch}-${build.configuration}$BETA

STAGEBASE=deb
STAGE=$STAGEBASE/$FINALNAME
STAGETARGET=$STAGE/$TARGET
BINSTAGE=$STAGE$BINTARGET
BGSTAGE=$STAGE$BGTARGET
HELPSTAGE=$STAGE$HELPTARGET
ICONSTAGE=$STAGE$ICONTARGET
LIBSTAGE=$STAGE$LIBTARGET

CLIENT_BIN_PATH=${libdir}/bin/${build.configuration}
CLIENT_IMAGEFORMATS_PATH=$CLIENT_BIN_PATH/imageformats
CLIENT_PLATFORMINPUTCONTEXTS_PATH=$CLIENT_BIN_PATH/platforminputcontexts
CLIENT_VOX_PATH=$CLIENT_BIN_PATH/vox
CLIENT_PLATFORMS_PATH=$CLIENT_BIN_PATH/platforms
CLIENT_BG_PATH=${libdir}/backgrounds
CLIENT_HELP_PATH=${ClientHelpSourceDir}
ICONS_PATH=${customization.dir}/icons/hicolor
CLIENT_LIB_PATH=${libdir}/lib/${build.configuration}

#. $CLIENT_BIN_PATH/env.sh

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE/imageformats
mkdir -p $BINSTAGE/platforminputcontexts
mkdir -p $HELPSTAGE
mkdir -p $LIBSTAGE
mkdir -p $BGSTAGE
mkdir -p $ICONSTAGE
mkdir -p "$STAGE/etc/xdg/$FULL_COMPANY_NAME"
mv -f debian/client.conf $STAGE/etc/xdg/"$FULL_COMPANY_NAME"/"$FULL_PRODUCT_NAME"
mv -f debian/applauncher.conf $STAGE/etc/xdg/"$FULL_COMPANY_NAME"/"$FULL_APPLAUNCHER_NAME"
mv -f usr/share/applications/icon.desktop usr/share/applications/${installer.name}.desktop

# Copy client binary, old version libs
cp -r $CLIENT_BIN_PATH/client.bin $BINSTAGE/client-bin
cp -r $CLIENT_BIN_PATH/applauncher $BINSTAGE/applauncher-bin
cp -r bin/applauncher $BINSTAGE

# Copy icons
cp -P -Rf usr $STAGE
cp -P -Rf $ICONS_PATH $ICONSTAGE
for f in `find $ICONSTAGE -name *.png`; do mv $f `dirname $f`/`basename $f .png`-${customization}.png; done

# Copy help
cp -r $CLIENT_HELP_PATH/* $HELPSTAGE

# Copy backgrounds
cp -r $CLIENT_BG_PATH/* $BGSTAGE

# Copy libraries, imageformats
cp -r $CLIENT_LIB_PATH/*.so* $LIBSTAGE
cp -r $CLIENT_PLATFORMINPUTCONTEXTS_PATH/*.* $BINSTAGE/platforminputcontexts
cp -r $CLIENT_IMAGEFORMATS_PATH/*.* $BINSTAGE/imageformats
cp -r $CLIENT_VOX_PATH $BINSTAGE
cp -r $CLIENT_PLATFORMS_PATH $BINSTAGE
rm -f $LIBSTAGE/*.debug

#copying qt libs
QTLIBS=`readelf -d $CLIENT_BIN_PATH/client.bin $CLIENT_PLATFORMS_PATH/libqxcb.so | grep libQt5 | sed -e 's/.*\(libQt5.*\.so\).*/\1/' | sort -u`
for var in $QTLIBS
do
    cp -P ${qt.dir}/lib/$var* $LIBSTAGE
done

cp -r /usr/lib/${arch.dir}/libXss.so.1* $LIBSTAGE
cp -r /lib/${arch.dir}/libpng12.so* $LIBSTAGE
cp -r /usr/lib/${arch.dir}/libopenal.so.1* $LIBSTAGE
#'libstdc++.so.6 is needed on some machines
cp -r /usr/lib/${arch.dir}/libstdc++.so.6* $LIBSTAGE
cp -P ${qt.dir}/lib/libicu*.so* $LIBSTAGE

find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
find $PKGSTAGE -type f -print0 | xargs -0 chmod 644

chmod 755 $BINSTAGE/*

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control

install -m 755 debian/prerm $STAGE/DEBIAN
install -m 755 debian/preinst $STAGE/DEBIAN
install -m 755 debian/postinst $STAGE/DEBIAN
install -m 644 debian/templates $STAGE/DEBIAN

(cd $STAGE; find * -type f -not -regex '^DEBIAN/.*' -print0 | xargs -0 md5sum > DEBIAN/md5sums; chmod 644 DEBIAN/md5sums)

(cd $STAGEBASE; fakeroot dpkg-deb -b $FINALNAME)
cp -r bin/update.json $STAGETARGET
echo "client.finalName=$FINALNAME" >> finalname-client.properties
echo "zip -y -r client-update-${platform}-${arch}-${release.version}.${buildNumber}.zip $STAGETARGET"
cd $STAGETARGET
zip -y -r client-update-${platform}-${arch}-${release.version}.${buildNumber}.zip ./*
mv -f client-update-${platform}-${arch}-${release.version}.${buildNumber}.zip ${project.build.directory}
