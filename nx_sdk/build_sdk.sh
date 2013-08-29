#!/bin/bash

exec > build.log
exec 2>&1

SDK_NAME=nx_sdk
TARGET_DIR=$SDK_NAME
VERSION=1.6.0

if [ -e $TARGET_DIR ]; then
  rm -rf $TARGET_DIR
fi

mkdir -p $TARGET_DIR/include/plugins/
cp -f readme.txt $TARGET_DIR

#Copying integration headers
cp -f ../common/src/plugins/plugin_api.h $TARGET_DIR/include/plugins/
cp -f ../common/src/plugins/camera_plugin.h $TARGET_DIR/include/plugins/

#Copying AXIS plugin
mkdir -p $TARGET_DIR/sample/axiscamplugin/src
cp -f ../axiscamplugin/*.pro $TARGET_DIR/sample/axiscamplugin/
cp -f ../axiscamplugin/*.pro $TARGET_DIR/sample/axiscamplugin/
cp -f ../axiscamplugin/src/*.h $TARGET_DIR/sample/axiscamplugin/src/
cp -f ../axiscamplugin/src/*.cpp $TARGET_DIR/sample/axiscamplugin/src/
cp -f ../axiscamplugin/Doxyfile $TARGET_DIR/sample/axiscamplugin/Doxyfile

#Removing unnecessary source files
rm -rf $TARGET_DIR/sample/axiscamplugin/src/compatibility_info.cpp

#Modifying paths in Doxyfile 
sed -i -e 's/common\/src/..\/include/g' ./nx_sdk/sample/axiscamplugin/Doxyfile

#Generating documentation
CUR_DIR_BAK=`pwd`
cd $TARGET_DIR/sample/axiscamplugin/
doxygen
cd $CUR_DIR_BAK
rm -rf $TARGET_DIR/sample/axiscamplugin/Doxyfile

#packing sdk
tar czf $SDK_NAME-$VERSION.tar.gz $TARGET_DIR
rm -rf $TARGET_DIR
