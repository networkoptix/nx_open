#!/bin/bash

#exec > build.log
#exec 2>&1

DOXYCHECK=`command -v doxygen`

SDK_NAME=nx_sdk
TARGET_DIR=$SDK_NAME
VERSION=1.6.0

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

mkdir -p $TARGET_DIR/include/plugins/
cp -f readme.txt $TARGET_DIR

#Copying integration headers
cp -f ../nxlib/nx_sdk/src/plugins/plugin_api.h $TARGET_DIR/include/plugins/
cp -f ../nxlib/nx_sdk/src/plugins/plugin_tools.h $TARGET_DIR/include/plugins/
cp -f ../nxlib/nx_sdk/src/plugins/plugin_container_api.h $TARGET_DIR/include/plugins/
cp -f ../nxlib/nx_sdk/src/camera/camera_plugin.h $TARGET_DIR/include/plugins/
cp -f ../nxlib/nx_sdk/src/camera/camera_plugin_types.h $TARGET_DIR/include/plugins/

PLUGINS=(axiscamplugin image_library_plugin rpi_cam)

for (( i=0; i<${#PLUGINS[@]}; i++ ))
do
    pluginName="${PLUGINS[$i]}"

    #Copying plugin
    mkdir -p $TARGET_DIR/sample/$pluginName/src
    cp -f ../plugins/$pluginName/src/*.txt $TARGET_DIR/sample/$pluginName/src/
    cp -f ../plugins/$pluginName/src/*.h $TARGET_DIR/sample/$pluginName/src/
    cp -f ../plugins/$pluginName/src/*.cpp $TARGET_DIR/sample/$pluginName/src/
    cp -f ../plugins/$pluginName/Doxyfile $TARGET_DIR/sample/$pluginName/Doxyfile
    cp -f ../plugins/$pluginName/readme.txt $TARGET_DIR/sample/$pluginName/readme.txt

    if [ $? -eq 0 ]
    then
        echo "sources copied"
    else
        echo "Error occured"
        exit 1
    fi

    #Removing unnecessary source files
    rm -rf $TARGET_DIR/sample/$pluginName/src/compatibility_info.cpp

    #Modifying paths in Doxyfile
    sed -i -e 's/common\/src/..\/include/g' ./nx_sdk/sample/$pluginName/Doxyfile

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
done

#packing sdk
#tar czf $SDK_NAME-$VERSION.tar.gz $TARGET_DIR
#rm -rf $TARGET_DIR
