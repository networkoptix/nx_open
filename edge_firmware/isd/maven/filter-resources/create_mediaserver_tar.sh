#!/bin/bash

set -e

# SRC_DIR=../../..


# function printHelp()
# {
#     echo "--target-dir={dir to copy packet to}"
#     echo
# }


CUSTOMIZATION=${deb.customization.company.name}
PRODUCT_NAME=${product.name.short}
MODULE_NAME=mediaserver
VERSION=${release.version}.${buildNumber}
MAJOR_VERSION="${parsedVersion.majorVersion}"
MINOR_VERSION="${parsedVersion.minorVersion}"
BUILD_VERSION="${parsedVersion.incrementalVersion}"

PACKAGE_NAME=${artifact.name.server}.tar.gz
UPDATE_NAME=${artifact.name.server_update}.zip

BUILD_DIR="`mktemp -d`"
PREFIX_DIR=/usr/local/apps/$CUSTOMIZATION

BUILD_OUTPUT_DIR=${libdir}
LIBS_DIR=$BUILD_OUTPUT_DIR/lib/${build.configuration}

STRIP=${packages.dir}/${rdep.target}/gcc-${gcc.version}/bin/arm-linux-gnueabihf-strip


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
( libavcodec.so \
libavdevice.so \
libavfilter.so \
libavformat.so \
libavutil.so \
libudt.so \
libcommon.so \
libcloud_db_client.so \
libnx_fusion.so \
libnx_network.so \
libnx_streaming.so \
libnx_utils.so \
libnx_email.so \
libappserver2.so \
libmediaserver_core.so \
libquazip.so \
libsasl2.so \
liblber-2.4.so.2 \
libldap-2.4.so.2 \
libldap_r-2.4.so.2 \
libsigar.so \
libswresample.so \
libswscale.so )

if [ -e "$LIBS_DIR/libvpx.so" ]; then
  LIBS_TO_COPY+=( libvpx.so )
fi
if [ -e "$LIBS_DIR/libcreateprocess.so" ]; then
  LIBS_TO_COPY+=( libcreateprocess.so )
fi

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR/$PREFIX_DIR
echo "$VERSION" > $BUILD_DIR/$PREFIX_DIR/version.txt

#copying libs
mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/
for var in "${LIBS_TO_COPY[@]}"
do
  echo "Adding lib" $var
  cp $LIBS_DIR/${var}* $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/
  if [ ! -z "$STRIP" ]; then
     $STRIP $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/${var}
  fi
done

#copying qt libs
QTLIBS="Core Gui Xml XmlPatterns Concurrent Network Sql"
for var in $QTLIBS
do
    qtlib=libQt5$var.so
    echo "Adding Qt lib" $qtlib
    cp -P ${qt.dir}/lib/$qtlib* $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/
done

#copying bin
mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/
cp $BUILD_OUTPUT_DIR/bin/${build.configuration}/mediaserver $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/

mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/
cp $BUILD_OUTPUT_DIR/bin/${build.configuration}/plugins/libisd_native_plugin.so $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/
#pushd $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/plugins/
#ln -s libisd_native_plugin.so libisd_native_plugin.so
#popd

#conf
mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/etc/
cp usr/local/apps/networkoptix/$MODULE_NAME/etc/mediaserver.conf.template $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/etc/

#start script
mkdir -p $BUILD_DIR/etc/init.d/
install -m 755 ./etc/init.d/S99networkoptix-$MODULE_NAME $BUILD_DIR/etc/init.d/S99$CUSTOMIZATION-$MODULE_NAME
sed -i "s/\${customization}/$CUSTOMIZATION/" $BUILD_DIR/etc/init.d/S99$CUSTOMIZATION-$MODULE_NAME


#building package
pushd $BUILD_DIR
tar czf $PACKAGE_NAME .$PREFIX_DIR ./etc

if [ ! -z $TARGET_DIR ]; then
  cp $PACKAGE_NAME $TARGET_DIR
fi

popd

cp $BUILD_DIR/$PACKAGE_NAME .

set +e
cp -P $LIBS_DIR/*.debug ${project.build.directory}
cp -P $BUILD_OUTPUT_DIR/bin/${build.configuration}/*.debug ${project.build.directory}
cp -P $BUILD_OUTPUT_DIR/bin/${build.configuration}/plugins/*.debug ${project.build.directory}
tar czf ./$PACKAGE_NAME-debug-symbols.tar.gz ./*.debug
set -e
rm -Rf $BUILD_DIR

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
rm -Rf zip
