#!/bin/bash

set -e

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

TOOLCHAIN_ROOT=$environment/packages/${box}/gcc-${gcc.version}
#bananapi uses bpi toolchain
if [[ "${box}" == "bpi" || "${box}" == "bananapi" ]]; then
    TOOLCHAIN_ROOT=$environment/packages/bpi/gcc-${gcc.version}
fi
TOOLCHAIN_PREFIX=$TOOLCHAIN_ROOT/bin/arm-linux-gnueabihf-

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

TEMP_DIR="`mktemp -d`"
BUILD_DIR="$TEMP_DIR/hdw_"$BOX_NAME"_build_app.tmp"
DEBUG_DIR="$TEMP_DIR/hdw_"$BOX_NAME"_build_debug.tmp"
PREFIX_DIR=/opt/$CUSTOMIZATION

BUILD_OUTPUT_DIR=${libdir}
BINS_DIR=$BUILD_OUTPUT_DIR/bin/${build.configuration}
LIBS_DIR=$BUILD_OUTPUT_DIR/lib/${build.configuration}

STRIP=

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
libudt.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libcommon.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libcloud_db_client.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libnx_network.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libnx_streaming.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libnx_utils.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libnx_email.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libappserver2.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libmediaserver_core.so.$MAJOR_VERSION$MINOR_VERSION$BUILD_VERSION.0.0 \
libpostproc.so.52.0.100 \
libsigar.so \
libsasl2.so.3.0.0 \
liblber-2.4.so.2.10.5 \
libldap-2.4.so.2.10.5 \
libldap_r-2.4.so.2.10.5 \
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
    LINK_TARGET="`echo $var | sed 's/\(.*so.[0-9]\+\)\(.*\)/\1/'`"
    ln -s $var $LINK_TARGET
done
popd

#copying qt libs
QTLIBS="Core Gui Xml XmlPatterns Concurrent Network Multimedia Sql"
for var in $QTLIBS
do
    qtlib=libQt5$var.so
    echo "Adding Qt lib" $qtlib
    cp -P ${qt.dir}/lib/$qtlib* $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/
done

#copying bin
mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/
mkdir -p $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/bin/
cp $BINS_DIR/mediaserver $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/
cp $BINS_DIR/external.dat $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/
if [ ! -z "$STRIP" ]; then
  $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/mediaserver $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/bin/mediaserver.debug
  $TOOLCHAIN_PREFIX"objcopy" --add-gnu-debuglink=$DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/bin/mediaserver.debug $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/mediaserver
  $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/mediaserver
fi

#copying plugins
if [ -e "$BINS_DIR/plugins" ]; then
  mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins
  mkdir -p $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins
  cp $BINS_DIR/plugins/*.* $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/
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

#start script and platform specific scripts
cp -R ./etc $BUILD_DIR
cp -R ./opt $BUILD_DIR


#additional platform specific files
if [[ "${box}" == "bpi" ]]; then
    cp -R ./root $BUILD_DIR
    mkdir -p $BUILD_DIR/root/tools/nx
    cp ./opt/networkoptix/$MODULE_NAME/etc/mediaserver.conf $BUILD_DIR/root/tools/nx
fi
if [ ! "$CUSTOMIZATION" == "networkoptix" ]; then
    mv -f $BUILD_DIR/etc/init.d/networkoptix-$MODULE_NAME $BUILD_DIR/etc/init.d/$CUSTOMIZATION-$MODULE_NAME
    cp -Rf $BUILD_DIR/opt/networkoptix/* $BUILD_DIR/opt/$CUSTOMIZATION
    rm -Rf $BUILD_DIR/opt/networkoptix/
fi

if [[ "${box}" == "bpi" || "${box}" == "bananapi" ]]; then
    cp -f -P $TOOLCHAIN_ROOT/arm-linux-gnueabihf/lib/libstdc++.s* $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib
    cp -f -P $environment/packages/bpi/opengl-es-mali/lib/* $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib
fi

chmod -R 755 $BUILD_DIR/etc/init.d
if [[ "${box}" == "bpi" ]]; then
    chmod -R 755 $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/var/scripts
fi

#building package

pushd $BUILD_DIR
    if [[ "${box}" == "bpi" ]]; then
        tar czf $PACKAGE_NAME .$PREFIX_DIR ./etc ./root
    else
        tar czf $PACKAGE_NAME .$PREFIX_DIR ./etc
    fi
    cp $PACKAGE_NAME ${project.build.directory}
popd

if [ ! -z "$STRIP" ]; then
    pushd $DEBUG_DIR/$PREFIX_DIR/$MODULE_NAME/
      tar czf $PACKAGE-debug-symbols.tar.gz ./bin ./lib
      cp $PACKAGE-debug-symbols.tar.gz ${project.build.directory}
    popd
fi

mkdir -p zip
mv $PACKAGE_NAME ./zip
mv update.* ./zip
mv install.sh ./zip
cd zip
if [ ! -f $PACKAGE_NAME ]; then
  echo "Distribution is not created! Exiting"
  exit 1
fi
zip ./$UPDATE_NAME.zip ./*
mv ./* ../
cd ..
rm -Rf zip
rm -Rf $TEMP_DIR
