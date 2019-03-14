#!/bin/bash

#exec > build.log
#exec 2>&1

DOXYCHECK=`command -v doxygen`

PLUGIN_NAME=ftpstorageplugin
TARGET_DIR=../nx_storage_sdk-build
NX_SDK_DIR=vms/libs/nx_sdk
PLUGIN_DIR=vms/server/storage_plugins/$PLUGIN_NAME

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

mkdir -p $TARGET_DIR/include/plugins
mkdir -p $TARGET_DIR/include/storage
mkdir -p $TARGET_DIR/sample/ftpstorageplugin

cp -f readme.txt $TARGET_DIR

#Copying integration headers
cp -f $NX_SDK_DIR/src/plugins/plugin_api.h $TARGET_DIR/include/plugins/
cp -f vms/libs/nx_sdk/src/storage/third_party_storage.h $TARGET_DIR/include/storage/

#Copying Ftp Storage plugin
mkdir -p $TARGET_DIR/sample/$PLUGIN_NAME/src/impl

cp -f $PLUGIN_DIR/src/CMakeLists.txt $TARGET_DIR/sample/$PLUGIN_NAME/src/

cp -f $PLUGIN_DIR/src/ftp_library.h $TARGET_DIR/sample/$PLUGIN_NAME/src/
cp -f $PLUGIN_DIR/src/ftp_library.cpp $TARGET_DIR/sample/$PLUGIN_NAME/src/

cp -f $PLUGIN_DIR/src/impl/ftplib.cpp $TARGET_DIR/sample/$PLUGIN_NAME/src/impl/
cp -f $PLUGIN_DIR/src/impl/ftplib.h $TARGET_DIR/sample/$PLUGIN_NAME/src/impl/

cp -f $PLUGIN_DIR/Doxyfile $TARGET_DIR/sample/$PLUGIN_NAME/

if [ $? -eq 0 ]
then
echo "sources copied"
else
echo "Error occured"
exit 1
fi

#Generating documentation
CUR_DIR_BAK=`pwd`
cd $TARGET_DIR/sample/$PLUGIN_NAME/
doxygen

if [ $? -eq 0 ]
then
echo "Doxygen run completed for $PLUGIN_NAME"
else
echo "Doxygen run failed for $PLUGIN_NAME"
exit 1
fi

cd $CUR_DIR_BAK
rm -rf $TARGET_DIR/sample/$PLUGIN_NAME/Doxyfile

echo "Built: $TARGET_DIR"