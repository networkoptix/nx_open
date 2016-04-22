#!/bin/bash

CONFIG=${build.configuration}
ARTIFACT=${project.artifactId}
PLATFORM=`uname -s`

DEBUG=

if [ $PLATFORM == 'Linux' ] && [ "${arch}" != "arm"  ]; then
    QTCHECK=`ldd ${qt.dir}/lib/libQt5Core.so | grep libglib-2.0.so.0`
    if [ -z "$QTCHECK" ]; then
	 echo '+++++++++++++ Invalid QT - does not support libglib. Compilation terminated +++++++++++++'
         exit 1
    fi
fi    

if [ "${box}" == "rpi" ] || [ "${box}" == "bpi" ]; then
     QTCHECK_ARM=`readelf -d ${qt.dir}/lib/libQt5Core.so | grep libglib-2.0.so.0`
     if [ -z "$QTCHECK_ARM" ]; then
         echo '+++++++++++++ Invalid QT - does not support libglib. Compilation terminated +++++++++++++'
         exit 1
     fi
     echo '+++++++++++++ QT for ${box} is valid +++++++++++++'
fi

export LD_LIBRARY_PATH=${libdir}/lib/${build.configuration}:${qt.dir}/lib
export DYLD_LIBRARY_PATH=${libdir}/lib/${build.configuration}
export DYLD_FRAMEWORK_PATH=${qt.dir}/lib

if [ "${platform}" == "android" ]; then
    export ANDROID_SDK_ROOT=${android.sdk}
    export ANDROID_NDK_ROOT=${android.ndk}
fi

case `uname -s` in
    "Linux")
        PLATFORM=linux
        NPROCESSORS=$[$(cat /proc/cpuinfo | grep ^processor | wc -l)]
        ;;
    "Darwin")
        PLATFORM=mac
        NPROCESSORS=`sysctl hw.ncpu | awk '{print $2}'`
        ;;
esac
if [ '${box}' == 'isd' ] || [ '${box}' == 'isd_s2' ]; then
    if [ '${project.artifactId}' == 'common' ] || [ '${project.artifactId}' == 'mediaserver_core' ]  || [ '${project.artifactId}' == 'appserver2' ] || [ '${project.artifactId}' == 'isd_native_plugin' ]; then
    #if [ '${project.artifactId}' == 'common' ] || [ '${project.artifactId}' == 'appserver2' ] || [ '${project.artifactId}' == 'isd_native_plugin' ]; then
        DEBUG=ext_debug
    fi
fi

make --no-p QUIET=yes -f Makefile.$CONFIG -j $[NPROCESSORS+1] $DEBUG || exit 1
