#!/bin/bash

# SRC_DIR=../../..


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



CUSTOMIZATION=${deb.customization.company.name}
PRODUCT_NAME=${product.name.short}
MODULE_NAME=mediaserver
VERSION=${release.version}.${buildNumber}
MAJOR_VERSION="${parsedVersion.majorVersion}"
MINOR_VERSION="${parsedVersion.minorVersion}"
BUILD_VERSION="${parsedVersion.incrementalVersion}"

BOX_NAME=${box}

PACKAGE=$CUSTOMIZATION-$MODULE_NAME-$VERSION-$BOX_NAME-beta
PACKAGE_NAME=$PACKAGE.tar.gz

BUILD_DIR=/tmp/hdw_$BOX_NAME_build.tmp
PREFIX_DIR=/usr/local/apps/$CUSTOMIZATION

BUILD_OUTPUT_DIR=${libdir}
LIBS_DIR=$BUILD_OUTPUT_DIR/lib/${build.configuration}

STRIP="`find ${root.dir}/mediaserver/ -name 'Makefile*' | head -n 1 | xargs grep -E 'STRIP\s+=' | cut -d= -f 2 | tr -d ' '`"


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
libquazip.so.1.0.0 \
libsigar.so \
libswresample.so.0.15.100 \
libswscale.so.2.1.100 )

if [ -e "$LIBS_DIR/libvpx.so.1.2.0" ]; then
  LIBS_TO_COPY+=( libvpx.so.1.2.0 )
fi


rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR/$PREFIX_DIR
echo "$VERSION" > $BUILD_DIR/$PREFIX_DIR/version.txt

#copying libs
mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/
for var in "${LIBS_TO_COPY[@]}"
do
  cp $LIBS_DIR/${var}   $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/
  if [ ! -z "$STRIP" ]; then
     echo $STRIP
     $STRIP $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/lib/${var}
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
cp $BUILD_OUTPUT_DIR/bin/${build.configuration}/mediaserver $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/

mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/
cp $BUILD_OUTPUT_DIR/bin/${build.configuration}/plugins/libisd_native_plugin.so $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/bin/plugins/
#pushd $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/plugins/
#ln -s libisd_native_plugin.so libisd_native_plugin.so
#popd

#conf
mkdir -p $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/etc/
cp ./usr/local/apps/networkoptix/$MODULE_NAME/etc/mediaserver.conf $BUILD_DIR/$PREFIX_DIR/$MODULE_NAME/etc/

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
cp -P $LIBS_DIR/*.debug ${project.build.directory}
cp -P $BUILD_OUTPUT_DIR/bin/${build.configuration}/*.debug ${project.build.directory}
cp -P $BUILD_OUTPUT_DIR/bin/${build.configuration}/plugins/*.debug ${project.build.directory}
tar czf ./$PACKAGE_NAME-debug-symbols.tar.gz ./*.debug
rm -Rf $BUILD_DIR

mkdir -p zip
mv $PACKAGE_NAME ./zip
mv update.* ./zip
mv install.sh ./zip
cd zip
zip ./$PACKAGE.zip ./*
mv ./* ../
cd ..
rm -Rf zip
