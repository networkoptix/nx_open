#!/bin/bash

COMPANY_NAME=${deb.customization.company.name}

PACKAGENAME=$COMPANY_NAME-client
VERSION=${release.version}
ARCHITECTURE=${os.arch}

TARGET=/opt/$COMPANY_NAME/client
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
USRTARGET=/usr
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

STAGEBASE=deb
STAGE=$STAGEBASE/${PACKAGENAME}-$VERSION.${buildNumber}-${arch}-${build.configuration}-beta
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET

CLIENT_BIN_PATH=${libdir}/bin/${build.configuration}
CLIENT_HELP_PATH=${libdir}/bin/help
CLIENT_STYLES_PATH=$CLIENT_BIN_PATH/styles
CLIENT_IMAGEFORMATS_PATH=$CLIENT_BIN_PATH/imageformats
CLIENT_SQLDRIVERS_PATH=$CLIENT_BIN_PATH/sqldrivers
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

# Copy client binary, x264
cp -r $CLIENT_BIN_PATH/client-bin $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
cp -r $CLIENT_BIN_PATH/applauncher-bin $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
cp -r $CLIENT_BIN_PATH/x264 $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
cp -r $CLIENT_BIN_PATH/x264 $BINSTAGE/1.4
cp -r $CLIENT_BIN_PATH/x264 $BINSTAGE/1.5
cp -r $CLIENT_BIN_PATH/vox $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}
cp -r ${project.build.directory}/1.4/bin/client-bin $BINSTAGE/1.4
cp -r ${project.build.directory}/1.5/bin/client-bin $BINSTAGE/1.5
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

# Copy libraries, styles, imageformats
cp -r $CLIENT_LIB_PATH/*.so* $LIBSTAGE
cp -r $CLIENT_STYLES_PATH/*.* $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}/styles
cp -r $CLIENT_STYLES_PATH/*.* $BINSTAGE/1.4/styles
cp -r $CLIENT_IMAGEFORMATS_PATH/*.* $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}/imageformats
cp -r $CLIENT_IMAGEFORMATS_PATH/*.* $BINSTAGE/1.4/imageformats
cp -r $CLIENT_SQLDRIVERS_PATH/*.* $BINSTAGE/${parsedVersion.majorVersion}.${parsedVersion.minorVersion}/sqldrivers

for f in `find $LIBSTAGE -type f` `find $BINSTAGE/**/styles -type f` $BINSTAGE/**/client-bin
do
    strip $f
    chrpath -d $f
done

find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
find $PKGSTAGE -type f -print0 | xargs -0 chmod 644
chmod 755 $BINSTAGE/1.*/* $BINSTAGE/*.*

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

(cd $STAGE; md5sum `find * -type f | grep -v '^DEBIAN/'` > DEBIAN/md5sums; chmod 644 DEBIAN/md5sums)

sudo chown -R root:root $STAGEBASE

(cd $STAGEBASE; sudo dpkg-deb -b ${PACKAGENAME}-$VERSION.${buildNumber}-${arch}-${build.configuration}-beta)
