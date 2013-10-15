#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}

PACKAGENAME=$COMPANY_NAME-client
VERSION=${release.version}
ARCHITECTURE=${os.arch}

TARGET=/opt/$COMPANY_NAME/client
BINTARGET=$TARGET/bin
BGTARGET=$TARGET/share/pictures/sample-backgrounds
LIBTARGET=$TARGET/lib
USRTARGET=/usr
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

STAGEBASE=deb
STAGE=$STAGEBASE/${PACKAGENAME}-$VERSION.${buildNumber}-${arch}-${build.configuration}-beta
BINSTAGE=$STAGE$BINTARGET
BGSTAGE=$STAGE$BGTARGET
LIBSTAGE=$STAGE$LIBTARGET

CLIENT_BIN_PATH=${libdir}/bin/${build.configuration}
CLIENT_HELP_PATH=${libdir}/bin/help
CLIENT_STYLES_PATH=$CLIENT_BIN_PATH/styles
CLIENT_IMAGEFORMATS_PATH=$CLIENT_BIN_PATH/imageformats
#CLIENT_SQLDRIVERS_PATH=$CLIENT_BIN_PATH/sqldrivers
CLIENT_BG_PATH=${libdir}/../../client/resource/common/backgrounds
CLIENT_LIB_PATH=${libdir}/build/bin/${build.configuration}

. $CLIENT_BIN_PATH/env.sh

# Prepare stage dir
rm -rf $STAGEBASE
mkdir -p $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}/styles
mkdir -p $BINSTAGE/1.4/styles
mkdir -p $BINSTAGE/1.5/styles
mkdir -p $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}/imageformats
mkdir -p $BINSTAGE/1.4/imageformats
mkdir -p $BINSTAGE/1.5/imageformats
mkdir -p $LIBSTAGE
mkdir -p $BGSTAGE

# Copy client binary, x264, old version libs
cp -r $CLIENT_BIN_PATH/client-bin $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
cp -r $CLIENT_BIN_PATH/applauncher-bin $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
cp -r $CLIENT_BIN_PATH/x264 $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
cp -r $CLIENT_BIN_PATH/x264 $BINSTAGE/1.4
cp -r $CLIENT_BIN_PATH/x264 $BINSTAGE/1.5
cp -r $CLIENT_BIN_PATH/vox $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
cp -r ${project.build.directory}/1.4/bin/client-bin $BINSTAGE/1.4
cp -r ${project.build.directory}/1.5/bin/client-bin $BINSTAGE/1.5
cp ${project.build.directory}/1.5/lib/*.* $LIBSTAGE
cp -r $CLIENT_BIN_PATH/x264 $BINSTAGE/1.4
cp -r $CLIENT_BIN_PATH/x264 $BINSTAGE/1.5
cp -r ${project.build.directory}/bin/applauncher* $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}

# Copy client startup script
#cp bin/client $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
#cp bin/client $BINSTAGE/1.4
#cp bin/client $BINSTAGE/1.5

# Copy icons
cp -P -Rf usr $STAGE

# Copy help
cp -r $CLIENT_HELP_PATH $BINSTAGE

# Copy backgrounds
cp -r $CLIENT_BG_PATH/* $BGSTAGE

# Copy libraries, styles, imageformats
cp -r $CLIENT_LIB_PATH/*.so* $LIBSTAGE
cp -r $CLIENT_STYLES_PATH/*.* $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}/styles
cp -r $CLIENT_STYLES_PATH/*.* $BINSTAGE/1.4/styles
cp -r $CLIENT_STYLES_PATH/*.* $BINSTAGE/1.5/styles
cp -r $CLIENT_IMAGEFORMATS_PATH/*.* $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}/imageformats
cp -r $CLIENT_IMAGEFORMATS_PATH/*.* $BINSTAGE/1.4/imageformats
cp -r $CLIENT_IMAGEFORMATS_PATH/*.* $BINSTAGE/1.5/imageformats
#cp -r $CLIENT_SQLDRIVERS_PATH/*.* $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}/sqldrivers

find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
find $PKGSTAGE -type f -print0 | xargs -0 chmod 644
chmod 755 $BINSTAGE/1.*/* 
chmod 755 $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}/*

# Must use system libraries due to compatibility issues
# cp -P ${qt.dir}/libaudio.so* $LIBSTAGE
# cp -P ${qt.dir}/libXi.so* $LIBSTAGE
# cp -P ${qt.dir}/libXt.so* $LIBSTAGE
# cp -P ${qt.dir}/libXrender.so* $LIBSTAGE
# cp -P ${qt.dir}/libfontconfig.so* $LIBSTAGE
# cp -P ${qt.dir}/libICE.so* $LIBSTAGE
# cp -P ${qt.dir}/libSM.so* $LIBSTAGE

# Prepare DEBIAN dir
mkdir -p $STAGE/DEBIAN

INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

cat debian/control.template | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" | sed "s/VERSION/$VERSION/g" | sed "s/ARCHITECTURE/$ARCHITECTURE/g" > $STAGE/DEBIAN/control

install -m 755 debian/prerm $STAGE/DEBIAN

(cd $STAGE; find * -type f -not -regex '^DEBIAN/.*' -print0 | xargs -0 md5sum > DEBIAN/md5sums; chmod 644 DEBIAN/md5sums)

(cd $STAGEBASE; fakeroot dpkg-deb -b ${PACKAGENAME}-$VERSION.${buildNumber}-${arch}-${build.configuration}-beta)
