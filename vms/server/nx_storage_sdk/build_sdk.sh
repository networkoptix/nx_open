#!/bin/bash

#exec > build.log
#exec 2>&1

DOXYCHECK=`command -v doxygen`

SDK_NAME=nx_storage_sdk
TARGET_DIR=$SDK_NAME
VERSION=1.6.0
NX_SDK_DIR=../../../libs/nx_sdk

if [ -z "$DOXYCHECK" ]; then
     sudo apt-get install -y doxygen
     if [ $? -eq 0 ]
     then
       echo "doxygen installed"
     else
       echo "Please install doxygen manually"
       exit 1
     fi
fi

if [ -e $TARGET_DIR ]; then
  rm -rf $TARGET_DIR
fi

mkdir -p $TARGET_DIR/include/plugins/storage/third_party
mkdir -p $TARGET_DIR/sample/ftpstorageplugin

cp -f readme.txt $TARGET_DIR

#Copying integration headers
cp -f $NX_SDK_DIR/src/plugins/plugin_api.h $TARGET_DIR/include/plugins/
cp -f ../vms/server/nx_vms_server/src/plugins/storage/third_party/third_party_storage.h $TARGET_DIR/include/plugins/storage/third_party

pluginName=ftpstorageplugin

#Copying Ftp Storage plugin
mkdir -p $TARGET_DIR/sample/$pluginName/src/impl

cp -f ../storage_plugins/$pluginName/src/CMakeLists.txt $TARGET_DIR/sample/$pluginName/src

cp -f ../storage_plugins/$pluginName/src/ftp_library.h $TARGET_DIR/sample/$pluginName/src
cp -f ../storage_plugins/$pluginName/src/ftp_library.cpp $TARGET_DIR/sample/$pluginName/src

cp -f ../storage_plugins/$pluginName/src/impl/ftplib.cpp $TARGET_DIR/sample/$pluginName/src/impl
cp -f ../storage_plugins/$pluginName/src/impl/ftplib.h $TARGET_DIR/sample/$pluginName/src/impl

cp -f ../storage_plugins/$pluginName/Doxyfile $TARGET_DIR/sample/$pluginName

if [ $? -eq 0 ]
then
echo "sources copied"
else
echo "Error occured"
exit 1
fi

#Generating documentation
CUR_DIR_BAK=`pwd`
cd $TARGET_DIR/sample/$pluginName/
doxygen

if [ $? -eq 0 ]
then
echo "Doxygen run completed for $pluginName"
else
echo "Doxygen run failed for $pluginName"
exit 1
fi

cd $CUR_DIR_BAK
rm -rf $TARGET_DIR/sample/$pluginName/Doxyfile
