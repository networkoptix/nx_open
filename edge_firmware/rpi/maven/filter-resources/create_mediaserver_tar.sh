#!/bin/bash
set -e
help()
{
    echo "Options:"
    echo "--target-dir=<dir to copy archive to>"
    echo "--no-strip"
    echo "--no-client"
}

PACKAGES_ROOT=$environment/packages/${box}
TOOLCHAIN_ROOT=$environment/packages/${box}/gcc-${gcc.version}
# Bananapi uses bpi toolchain.
if [[ "${box}" == "bananapi" ]]; then
    PACKAGES_ROOT=$environment/packages/bpi
    TOOLCHAIN_ROOT=$environment/packages/bpi/gcc-${gcc.version}
fi
TOOLCHAIN_PREFIX=$TOOLCHAIN_ROOT/bin/arm-linux-gnueabihf-
SYSROOT_PREFIX=$PACKAGES_ROOT/sysroot/usr/lib/arm-linux-gnueabihf

CUSTOMIZATION=${deb.customization.company.name}
PRODUCT_NAME=${product.name.short}
MODULE_NAME=mediaserver
VERSION=${release.version}.${buildNumber}
MAJOR_VERSION="${parsedVersion.majorVersion}"
MINOR_VERSION="${parsedVersion.minorVersion}"
BUILD_VERSION="${parsedVersion.incrementalVersion}"

PACKAGE_NAME=${artifact.name.server}.tar.gz
UPDATE_NAME=${artifact.name.server_update}.zip

TEMP_DIR="`mktemp -d`"
BUILD_DIR="$TEMP_DIR/hdw_"$BOX_NAME"_build_app.tmp"
DEBUG_DIR="$TEMP_DIR/hdw_"$BOX_NAME"_build_debug.tmp"
PREFIX_DIR="/opt/$CUSTOMIZATION"
if [ "${box}" = "bpi" ]; then
    TARGET_LIB_DIR="$PREFIX_DIR/lib"
else
    TARGET_LIB_DIR="$PREFIX_DIR/mediaserver/lib"
fi

BUILD_OUTPUT_DIR=${libdir}
BINS_DIR=$BUILD_OUTPUT_DIR/bin/${build.configuration}
LIBS_DIR=$BUILD_OUTPUT_DIR/lib/${build.configuration}
DEBS_DIR=$BUILD_OUTPUT_DIR/deb
UBOOT_DIR=$BUILD_OUTPUT_DIR/root
USR_DIR=$BUILD_OUTPUT_DIR/usr
VOX_SOURCE_DIR=${ClientVoxSourceDir}

STRIP=
WITH_CLIENT=1

for i in "$@"; do
    if [ $i == "-h" -o $i == "--help"  ] ; then
        help
        exit 0
    elif [[ "$i" =~ "--target-dir=" ]] ; then
        TARGET_DIR="`echo $i | sed 's/--target-dir=\(.*\)/\1/'`"
    elif [ "$i" == "--no-strip" ] ; then
        STRIP=
    elif [ "$i" == "--no-client" ] ; then
        WITH_CLIENT=
    fi
done

LIBS_TO_COPY=(
    libavcodec
    libavdevice
    libavfilter
    libavformat
    libavutil
    liblber-2.4
    libldap-2.4
    libldap_r-2.4
    libquazip
    libsasl2
    libsigar
    libswresample
    libswscale
    libappserver2
    libcloud_db_client
    libcommon
    libmediaserver_core
    libnx_email
    libnx_fusion
    libnx_kit
    libnx_network
    libnx_utils
    libpostproc
    libudt
)

if [ "${box}" = "bpi" ] || [ "${box}" = "bananapi" ]; then
    LIBS_TO_COPY+=(
        # Put non-raspberry pi (bananapi, nx1) specific server libs here.
        libGLESv2
        libMali
        libUMP
    )
fi

# Additional libs for nx1.
if [ "${box}" = "bpi" ]; then
    LIBS_TO_COPY+=(
        # Put nx1(bpi) specific server libs here.
    )
    if [ ! -z "$WITH_CLIENT" ]; then
        LIBS_TO_COPY+=(
            ldpreloadhook
            libcedrus
            libnx_vms_utils
            libnx_client_core
            libnx_audio
            libnx_media
            libopenal
            libproxydecoder
            libEGL
            libGLESv1_CM
            libpixman-1
            libvdpau_sunxi
        )
    fi
fi

if [ -e "$LIBS_DIR/libvpx.so.1.2.0" ]; then
  LIBS_TO_COPY+=(libvpx.so)
fi

if [ ! "$CUSTOMIZATION" = "networkoptix" ]; then
    mv -f opt/networkoptix opt/$CUSTOMIZATION
fi
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR/$PREFIX_DIR
echo "$VERSION" >$BUILD_DIR/$PREFIX_DIR/version.txt

# Copy libs.
mkdir -p $BUILD_DIR/$TARGET_LIB_DIR/
mkdir -p $DEBUG_DIR/$TARGET_LIB_DIR/
for var in "${LIBS_TO_COPY[@]}"; do
    echo "Adding lib" ${var}
    cp $LIBS_DIR/${var}* $BUILD_DIR/$TARGET_LIB_DIR/ -av
    if [ ! -z "$STRIP" ]; then
        $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug $BUILD_DIR/$TARGET_LIB_DIR/${var} \
            $DEBUG_DIR/$TARGET_LIB_DIR/${var}.debug
        $TOOLCHAIN_PREFIX"objcopy" --add-gnu-debuglink=$DEBUG_DIR/$TARGET_LIB_DIR/${var}.debug \
            $BUILD_DIR/$TARGET_LIB_DIR/${var}
        $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$TARGET_LIB_DIR/${var}
    fi
done

# Copy qt libs.
QTLIBS="Core Gui Xml XmlPatterns Concurrent Network Multimedia Sql WebSockets"
if [ "${box}" = "bpi" ] && [ ! -z "$WITH_CLIENT" ]; then
    QTLIBS="Concurrent Core EglDeviceIntegration Gui LabsTemplates MultimediaQuick_p Multimedia Network Qml Quick Sql Xml XmlPatterns DBus Web*"
fi
for var in $QTLIBS; do
    qtlib=libQt5$var.so
    echo "Adding Qt lib" $qtlib
    cp -r ${qt.dir}/lib/$qtlib* $BUILD_DIR/$TARGET_LIB_DIR/
done

# Copy server bin.
mkdir -p $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/
mkdir -p $DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/
cp $BINS_DIR/mediaserver $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/
cp $BINS_DIR/external.dat $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/
if [ ! -z "$STRIP" ]; then
    $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug \
        $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/mediaserver \
        $DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/mediaserver.debug
    $TOOLCHAIN_PREFIX"objcopy" \
        --add-gnu-debuglink=$DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/mediaserver.debug \
        $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/mediaserver
    $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/mediaserver
fi

# Conf.
mkdir -p $BUILD_DIR/$PREFIX_DIR/mediaserver/etc/
cp opt/$CUSTOMIZATION/mediaserver/etc/mediaserver.conf.template \
    $BUILD_DIR/$PREFIX_DIR/mediaserver/etc

# Start script and platform-specific scripts.
cp -R ./etc $BUILD_DIR
cp -R ./opt $BUILD_DIR
# Copying debs.
cp -Rfv $DEBS_DIR $BUILD_DIR/opt

if [ "${box}" = "bpi" ] && [ ! -z "$WITH_CLIENT" ]; then
    # Copy ffmpeg 3.0.2 libs.
    cp -av $LIBS_DIR/ffmpeg $BUILD_DIR/$TARGET_LIB_DIR/
    # Copy lite_client bin.
    mkdir -p $BUILD_DIR/$PREFIX_DIR/lite_client/bin/
    mkdir -p $DEBUG_DIR/$PREFIX_DIR/lite_client/bin/
    cp $BINS_DIR/mobile_client $BUILD_DIR/$PREFIX_DIR/lite_client/bin/
    if [ ! -z "$STRIP" ]; then
        $TOOLCHAIN_PREFIX"objcopy" \
            --only-keep-debug $BUILD_DIR/$PREFIX_DIR/lite_client/bin/mobile_client \
            $DEBUG_DIR/$PREFIX_DIR/lite_client/bin/mobile_client.debug
        $TOOLCHAIN_PREFIX"objcopy" \
            --add-gnu-debuglink=$DEBUG_DIR/$PREFIX_DIR/lite_client/bin/mobile_client.debug \
            $BUILD_DIR/$PREFIX_DIR/lite_client/bin/mobile_client
        $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$PREFIX_DIR/lite_client/bin/mobile_client
    fi
    # Create symlink for rpath needed by mediaserver binary.
    ln -s "../lib" "$BUILD_DIR/$PREFIX_DIR/mediaserver/lib"

    # Create symlink for rpath needed by mobile_client binary.
    ln -s "../lib" "$BUILD_DIR/$PREFIX_DIR/lite_client/lib"

    # Create symlink for rpath needed by Qt plugins.
    ln -s "../../lib" "$BUILD_DIR/$PREFIX_DIR/lite_client/bin/lib"

    # Copy directories needed by lite client.
    DIRS_TO_COPY=(
        egldeviceintegrations
        fonts
        imageformats
        mobile_client
        platforms
        qml
        video
    )
    for d in "${DIRS_TO_COPY[@]}"; do
        echo "Copying directory ${d}"
        cp -Rfv $BINS_DIR/${d} $BUILD_DIR/$PREFIX_DIR/lite_client/bin
    done

    # Copying uboot.
    cp -Rfv $UBOOT_DIR $BUILD_DIR/root

    # Copy additional binaries.
    cp -Rfv $USR_DIR $BUILD_DIR/usr

    # Copy additional platform-specific files.
    cp -Rf ${qt.dir}/libexec $BUILD_DIR/$PREFIX_DIR/lite_client/bin
    mkdir -p $BUILD_DIR/$PREFIX_DIR/lite_client/bin/translations
    cp -Rf ${qt.dir}/translations $BUILD_DIR/$PREFIX_DIR/lite_client/bin
    cp -Rf ${qt.dir}/resources $BUILD_DIR/$PREFIX_DIR/lite_client/bin
    cp -f ${qt.dir}/resources/* $BUILD_DIR/$PREFIX_DIR/lite_client/bin/libexec
    cp -R ./root $BUILD_DIR
    mkdir -p $BUILD_DIR/root/tools/nx
    cp opt/$CUSTOMIZATION/mediaserver/etc/mediaserver.conf.template $BUILD_DIR/root/tools/nx
    if [ "${box}" = "bpi" ]; then
        chmod -R 755 $BUILD_DIR/$PREFIX_DIR/mediaserver/var/scripts
    fi
fi

# Copy plugins.
if [ -e "$BINS_DIR/plugins" ]; then
    mkdir -p $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins
    mkdir -p $DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/plugins
    cp $BINS_DIR/plugins/*.* $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins/
    for f in `ls $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins/`; do
        if [ ! -z "$STRIP" ]; then
            $TOOLCHAIN_PREFIX"objcopy" --only-keep-debug \
                $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins/${f} \
                $DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/plugins/${f}.debug
            $TOOLCHAIN_PREFIX"objcopy" \
                --add-gnu-debuglink=$DEBUG_DIR/$PREFIX_DIR/mediaserver/bin/plugins/${f}.debug \
                $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins/${f}
            $TOOLCHAIN_PREFIX"strip" -g $BUILD_DIR/$PREFIX_DIR/mediaserver/bin/plugins/${f}
        fi
    done
fi

# Copy additional sysroot.
if [ "${box}" = "bpi" ]; then
    SYSROOT_LIBS_TO_COPY+=(
        libopus
        libvpx
        libwebpdemux
        libwebp
    )
    for var in "${SYSROOT_LIBS_TO_COPY[@]}"; do
        echo "Adding lib $var"
        cp "$SYSROOT_PREFIX"/$var* "$BUILD_DIR/$TARGET_LIB_DIR/" -av
    done
elif [ "${box}" = "bananapi" ]; then
    # Add files required for bananapi on Debian 8 "Jessie".
    cp -r "$SYSROOT_PREFIX"/libglib* "$BUILD_DIR/$TARGET_LIB_DIR/"
    cp -r "$PACKAGES_ROOT/sysroot/usr/bin/hdparm" "$BUILD_DIR/$PREFIX_DIR/mediaserver/bin/"
fi

# Copy vox.
VOX_TARGET_DIR=$BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/vox
mkdir -p $VOX_TARGET_DIR
cp -Rf $VOX_SOURCE_DIR/* $VOX_TARGET_DIR

NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT="$BUILD_DIR/etc/init.d/networkoptix-mediaserver"
MEDIASERVER_STARTUP_SCRIPT="$BUILD_DIR/etc/init.d/$CUSTOMIZATION-mediaserver"
NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT="$BUILD_DIR/etc/init.d/networkoptix-lite-client"
LITE_CLIENT_STARTUP_SCRIPT="$BUILD_DIR/etc/init.d/$CUSTOMIZATION-lite-client"

if [ ! "$CUSTOMIZATION" = "networkoptix" ]; then
    if [ -f "$NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT" ]; then
        mv -f "$NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT" "$MEDIASERVER_STARTUP_SCRIPT"
    fi
    if [ -f "$NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT" ]; then
        mv -f "$NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT" "$LITE_CLIENT_STARTUP_SCRIPT"
    fi
fi

if [ "${box}" = "bpi" ] || [ "${box}" = "bananapi" ]; then
    # Uncomment to enable libc/libstdc++ upgrade.
    #cp -f -P $PACKAGES_ROOT/libstdc++-6.0.20/lib/libstdc++.s* $BUILD_DIR/$TARGET_LIB_DIR
    cp -f -P $PACKAGES_ROOT/libstdc++-6.0.19/lib/libstdc++.s* $BUILD_DIR/$TARGET_LIB_DIR
fi

chmod -R 755 $BUILD_DIR/etc/init.d

# Build package.

pushd $BUILD_DIR
if [ "${box}" = "bpi" ]; then
    if [ ! -z "$WITH_CLIENT" ]; then
        tar czf $PACKAGE_NAME ./opt ./etc ./root ./usr
    else
        tar czf $PACKAGE_NAME ./opt ./etc
    fi
else
    tar czf $PACKAGE_NAME ./opt ./etc
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
zip ./$UPDATE_NAME ./*
mv ./* ../
cd ..
rm -rf zip
rm -rf $BUILD_DIR
rm -rf $TEMP_DIR
rm -rf $DEBUG_DIR
