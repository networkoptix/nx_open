#!/bin/bash

set -e

# SRC_DIR=../../


# function printHelp()
# {
#     echo "--target-dir={dir to copy packet to}"
#     echo
# }


PACKAGES_ROOT=$environment/packages/${box}
TOOLCHAIN_ROOT=$environment/packages/${box}/gcc-${gcc.version}
#bananapi uses bpi toolchain
if [[ "${box}" == "bananapi" ]]; then
    PACKAGES_ROOT=$environment/packages/bpi
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
PACKAGE=$CUSTOMIZATION-mediaserver-$BOX_NAME-$VERSION
PACKAGE_NAME=$PACKAGE$BETA.tar.gz
UPDATE_NAME=server-update-$BOX_NAME-${arch}-$VERSION

TEMP_DIR="`mktemp -d`"
BUILD_DIR="$TEMP_DIR/hdw_"$BOX_NAME"_build_app.tmp"
DEBUG_DIR="$TEMP_DIR/hdw_"$BOX_NAME"_build_debug.tmp"
PREFIX_DIR=/opt/$CUSTOMIZATION
if [[ "${box}" == "bpi" ]]; then TARGET_LIB_DIR=$PREFIX_DIR/lib; else TARGET_LIB_DIR=$PREFIX_DIR/mediaserver/lib; fi

BUILD_OUTPUT_DIR=${libdir}
BINS_DIR=$BUILD_OUTPUT_DIR/bin/${build.configuration}
LIBS_DIR=$BUILD_OUTPUT_DIR/lib/${build.configuration}
DEBS_DIR=$BUILD_OUTPUT_DIR/deb
UBOOT_DIR=$BUILD_OUTPUT_DIR/root
VOX_SOURCE_DIR=${ClientVoxSourceDir}

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
( libavcodec \
libavdevice \
libavfilter \
libavformat \
libavutil \
liblber-2.4 \
libldap-2.4 \
libldap_r-2.4 \
libpostproc \
libquazip \
libsasl2 \
libsigar \
libswresample \
libswscale \
libappserver2 \
libcloud_db_client \
libcommon \
libmediaserver_core \
libnx_email \
libnx_fusion \
libnx_network \
libnx_streaming \
libnx_utils \
libnx_vms_utils \
libudt )

#additional libs for nx1 client
if [[ "${box}" == "bpi" ]]; then
    LIBS_TO_COPY+=( \
    ldpreloadhook \
    libcedrus \
    libclient.core \
    libnx_audio \
    libnx_media \
    libopenal \
    libproxydecoder \
    libEGL \
    libGLESv1_CM \
    libGLESv2 \
    libMali \
    libpixman-1 \
    libUMP \
    libvdpau_sunxi )
fi

if [ -e "$LIBS_DIR/libvpx.so.1.2.0" ]; then
  LIBS_TO_COPY+=( libvpx.so )
fi

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR/$PREFIX_DIR
echo "$VERSION" > $BUILD_DIR/$PREFIX_DIR/version.txt

#copying libs
mkdir -p $BUILD_DIR/$TARGET_LIB_DIR/
mkdir -p $DEBUG_DIR/$TARGET_LIB_DIR/
for var in "${LIBS_TO_COPY[@]}"
do
  echo "Adding lib" ${var}
  cp $LIBS_DIR/${var}* $BUILD_DIR/$TARGET_LIB_DIR/ -av
  if [ ! -z "$STRIP" ]; then
    $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug $BUILD_DIR/$TARGET_LIB_DIR/${var} $DEBUG_DIR/$TARGET_LIB_DIR/${var}.debug
    $TOOLCHAIN_PREFIX"objcopy" --add-gnu-debuglink=$DEBUG_DIR/$TARGET_LIB_DIR/${var}.debug $BUILD_DIR/$TARGET_LIB_DIR/${var}
    $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$TARGET_LIB_DIR/${var}
  fi
done

#copying qt libs
QTLIBS="Core Gui Xml XmlPatterns Concurrent Network Multimedia Sql"
if [[ "${box}" == "bpi" ]]; then
    QTLIBS="Concurrent Core EglDeviceIntegration Gui LabsTemplates MultimediaQuick_p Multimedia Network OpenGL Qml Quick Sql Widgets Xml XmlPatterns"
fi
for var in $QTLIBS
do
    qtlib=libQt5$var.so
    echo "Adding Qt lib" $qtlib
    cp -P ${qt.dir}/lib/$qtlib* $BUILD_DIR/$TARGET_LIB_DIR/
done

#copying server bin
mkdir -p $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/
mkdir -p $DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/
cp $BINS_DIR/mediaserver $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/
cp $BINS_DIR/external.dat $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/
if [ ! -z "$STRIP" ]; then
  $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/mediaserver $DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/mediaserver.debug
  $TOOLCHAIN_PREFIX"objcopy" --add-gnu-debuglink=$DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/mediaserver.debug $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/mediaserver
  $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/mediaserver
fi

#conf
mkdir -p $BUILD_DIR/$PREFIX_DIR/mediaserver/etc/
cp ./opt/networkoptix/mediaserver/etc/mediaserver.conf $BUILD_DIR/$PREFIX_DIR/mediaserver/etc

#start script and platform specific scripts
cp -R ./etc $BUILD_DIR
cp -R ./opt $BUILD_DIR

if [[ "${box}" == "bpi" ]]; then
  #copying ffmpeg 3.0.2 libs
  cp -av $LIBS_DIR/ffmpeg $BUILD_DIR/$TARGET_LIB_DIR/
  #copying lite client bin
  mkdir -p $BUILD_DIR/$PREFIX_DIR/lite_client/bin/
  mkdir -p $DEBUG_DIR/$PREFIX_DIR/lite_client/bin/
  cp $BINS_DIR/mobile_client $BUILD_DIR/$PREFIX_DIR/lite_client/bin/
    if [ ! -z "$STRIP" ]; then
    $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug $BUILD_DIR/$PREFIX_DIR/lite_client/bin/mobile_client $DEBUG_DIR/$PREFIX_DIR/lite_client/bin/mobile_client.debug
    $TOOLCHAIN_PREFIX"objcopy" --add-gnu-debuglink=$DEBUG_DIR/$PREFIX_DIR/lite_client/bin/mobile_client.debug $BUILD_DIR/$PREFIX_DIR/lite_client/bin/mobile_client
    $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$PREFIX_DIR/lite_client/bin/mobile_client
  fi

  #copying directories needed by lite client
  DIRS_TO_COPY=( \
  egldeviceintegrations \
  fonts \
  imageformats \
  mobile_client \
  platforms \
  qml \
  video \
  )
  for d in "${DIRS_TO_COPY[@]}"; do
    echo Copying directory ${d}
    cp -Rfv $BINS_DIR/${d} $BUILD_DIR/$PREFIX_DIR/lite_client/bin
  done

  #copying debs and uboot
  cp -Rfv $DEBS_DIR $BUILD_DIR/opt
  cp -Rfv $UBOOT_DIR $BUILD_DIR/root

  #additional platform specific files
  mkdir -p $BUILD_DIR/$PREFIX_DIR/lite_client/bin/lib
  cp -Rf ${qt.dir}/lib/fonts $BUILD_DIR/$PREFIX_DIR/lite_client/bin/lib
  cp -R ./root $BUILD_DIR
  mkdir -p $BUILD_DIR/root/tools/nx
  cp ./opt/networkoptix/mediaserver/etc/mediaserver.conf $BUILD_DIR/root/tools/nx
  chmod -R 755 $BUILD_DIR/$PREFIX_DIR/mediaserver/var/scripts
fi

#copying plugins
if [ -e "$BINS_DIR/plugins" ]; then
  mkdir -p $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins
  mkdir -p $DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/plugins
  cp $BINS_DIR/plugins/*.* $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins/
  for f in `ls $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins/`
    do
      if [ ! -z "$STRIP" ]; then
        $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins/${f} $DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/plugins/${f}.debug
        $TOOLCHAIN_PREFIX"objcopy" --add-gnu-debuglink=$DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/plugins/${f}.debug $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins/${f}
        $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins/${f}
      fi
    done
fi

#copying vox
VOX_TARGET_DIR=$BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/vox
mkdir -p $VOX_TARGET_DIR
cp -Rf $VOX_SOURCE_DIR/* $VOX_TARGET_DIR

if [ ! "$CUSTOMIZATION" == "networkoptix" ]; then
    mv -f $BUILD_DIR/etc/init.d/networkoptix-mediaserver $BUILD_DIR/etc/init.d/$CUSTOMIZATION-mediaserver
    cp -Rf $BUILD_DIR/opt/networkoptix/* $BUILD_DIR/opt/$CUSTOMIZATION
    rm -Rf $BUILD_DIR/opt/networkoptix/
fi

if [[ "${box}" == "bpi" || "${box}" == "bananapi" ]]; then
    cp -f -P $PACKAGES_ROOT/libstdc++-6.0.20/lib/libstdc++.s* $BUILD_DIR/$TARGET_LIB_DIR
fi

chmod -R 755 $BUILD_DIR/etc/init.d

#building package

pushd $BUILD_DIR
    if [[ "${box}" == "bpi" ]]; then
        tar czf $PACKAGE_NAME ./opt ./etc ./root
    else
        tar czf $PACKAGE_NAME .$PREFIX_DIR ./etc
    fi
    cp $PACKAGE_NAME ${project.build.directory}
popd

if [ ! -z "$STRIP" ]; then
    pushd $DEBUG_DIR/$PREFIX_DIR/mediaserver/
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
#rm -Rf $TEMP_DIR
