#!/bin/bash

# SRC_DIR=../../


# function printHelp()
# {
#     echo "--target-dir={dir to copy packet to}"
#     echo
# }

# function get_var()
# {
#     local h="`grep -R $1 $SRC_DIR/mediaserver/arm/version.h | sed 's/.*"\(.*\)".*/\1/'`"
#     if [[ "$1" == "QN_CUSTOMIZATION_NAME" && "$h" == "default" ]]; then
#         h=networkoptix
#     fi
#     echo "$h"
# }
TOOLCHAIN_PREFIX=/usr/local/raspberrypi-tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-


CUSTOMIZATION=${deb.customization.company.name}
PRODUCT_NAME=${product.name.short}
MODULE_NAME=mediaserver
VERSION=${release.version}.${buildNumber}
MAJOR_VERSION="${parsedVersion.majorVersion}"
MINOR_VERSION="${parsedVersion.minorVersion}"
BUILD_VERSION="${parsedVersion.incrementalVersion}"

BOX_NAME=${box}
BETA=""
if [[ "${beta}" == "true" ]]; then 
  BETA="-beta" 
fi 
PACKAGE=$CUSTOMIZATION-$MODULE_NAME-$BOX_NAME-$VERSION
PACKAGE_NAME=$PACKAGE$BETA.tar.gz
UPDATE_NAME=server-update-$BOX_NAME-${arch}-$VERSION

BUILD_DIR="/tmp/hdw_"$BOX_NAME"_build.tmp"
DEBUG_DIR="/tmp/hdw_"$BOX_NAME"_build_debug.tmp"
PREFIX_DIR=/opt/$CUSTOMIZATION

BUILD_OUTPUT_DIR=${libdir}
LIBS_DIR=$BUILD_OUTPUT_DIR/lib/release

STRIP=true

for i in "$@"
do
    if [ $i == "-h" -o $i == "--help"  ] ; then
        printHelp
        exit 0
    elif [[ "$i" =~ "--target-dir=" ]] ; then
        TARGET_DIR="`echo $i | sed 's/--target-dir=\(.*\)/\1/'`"
    elif [ "$i" == "--no-strip" ] ; then
        STRIP=
    fi
done

LIBS_TO_COPY=\
( libavcodec.so.54.23.100 \
libavdevice.so.54.0.100 \
libavfilter.so.2.77.100 \
libavformat.so.54.6.100 \
libavutil.so.51.54.100 \
libcommon.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libappserver2.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libpostproc.so.52.0.100 \
libQt5Concurrent.so.5.2.1 \
libQt5Core.so.5.2.1 \
libQt5Gui.so.5.2.1 \
libQt5Multimedia.so.5.2.1 \
libQt5Network.so.5.2.1 \
libQt5Sql.so.5.2.1 \
libQt5Xml.so.5.2.1 \
libQt5XmlPatterns.so.5.2.1 \
libsigar.so \
libswresample.so.0.15.100 \
libswscale.so.2.1.100 \
libquazip.so.1.0.0 )

if [ -e "$LIBS_DIR/libvpx.so.1.2.0" ]; then
  LIBS_TO_COPY+=( libvpx.so.1.2.0 )
fi

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR/$PREFIX_DIR
echo "$VERSION" > $BUILD_DIR/$PREFIX_DIR/version.txt

#copying libs
mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/
mkdir -p $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/lib/
for var in "${LIBS_TO_COPY[@]}"
do
  cp $LIBS_DIR/${var} $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/
  if [ ! -z "$STRIP" ]; then
    $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/${var} $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/lib/${var}.debug
    $TOOLCHAIN_PREFIX"objcopy" --add-gnu-debuglink=$DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/lib/${var}.debug $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/${var}
    $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/${var}
  fi
done

#generating links
pushd $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/
LIBS="`find ./ -name '*.so.*.*.*'`"
for var in $LIBS
do
    ln -s $var "`echo $var | cut -d . -f 1,2,3,4`"
done
popd


#copying bin
mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/
mkdir -p $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/bin/
cp $BUILD_OUTPUT_DIR/bin/release/mediaserver $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/
if [ ! -z "$STRIP" ]; then
  $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/mediaserver $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/bin/mediaserver.debug
  $TOOLCHAIN_PREFIX"objcopy" --add-gnu-debuglink=$DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/bin/mediaserver.debug $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/mediaserver
  $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/mediaserver
fi


#copying plugins
if [ -e "$BUILD_OUTPUT_DIR/bin/release/plugins" ]; then
  mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins
  mkdir -p $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins
  cp $BUILD_OUTPUT_DIR/bin/release/plugins/*.* $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/
  for f in `ls $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/`
    do
      if [ ! -z "$STRIP" ]; then
        $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/${f} $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/${f}.debug
        $TOOLCHAIN_PREFIX"objcopy" --add-gnu-debuglink=$DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/${f}.debug $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/${f}
        $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/${f}
      fi
    done
fi

#conf
mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/etc/
cp ./opt/networkoptix/$MODULE_NAME/etc/mediaserver.conf $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/etc

#start script
mkdir -p $BUILD_DIR/etc/init.d/
install -m 755 ./etc/init.d/networkoptix-$MODULE_NAME $BUILD_DIR/etc/init.d/$CUSTOMIZATION-$MODULE_NAME


#building package
pushd $BUILD_DIR
  tar czf $PACKAGE_NAME .$PREFIX_DIR ./etc
  cp $PACKAGE_NAME ${project.build.directory}
popd

pushd $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/
  tar czf $PACKAGE-debug-symbols.tar.gz ./bin ./lib
  cp $PACKAGE-debug-symbols.tar.gz ${project.build.directory}
popd

mkdir -p zip
mv $PACKAGE_NAME ./zip
mv update.* ./zip
mv install.sh ./zip
cd zip
zip ./$UPDATE_NAME.zip ./*
mv ./* ../
cd ..
rm -Rf zip