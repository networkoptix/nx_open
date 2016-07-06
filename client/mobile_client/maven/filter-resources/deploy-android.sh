#!/bin/bash

BUILD_TARGET=apk
ANDROID_NDK_ROOT=${env.environment}/android/android-ndk

if [ -d "$BUILD_TARGET/libs" ]; then
    rm -rf $BUILD_TARGET/libs
fi

APK_SUFFIX=
BUILD_TYPE=
SIGN=
SOURCE_APK=
if [[ "${build.configuration}" == "release" ]]; then
    BUILD_TYPE=--release
    SIGN="--sign ${google.keystore} ${google.alias} --storepass ${google.storepass} --keypass ${google.keypass}"
    SOURCE_APK=apk/bin/QtApp-release-signed.apk

    if [[ "${beta}" == "true" ]]; then
        APK_SUFFIX="beta"
    else
        APK_SUFFIX="release"
    fi
else
    SOURCE_APK=apk/bin/QtApp-debug.apk
    APK_SUFFIX="debug"
fi
TARGET_APK=revamped-${installer.name}-${project.version.name}.${project.version.code}-$APK_SUFFIX.apk

rm -rf $BUILD_TARGET

set -e

make install --makefile=Makefile.${build.configuration} INSTALL_ROOT=$BUILD_TARGET
${qt.dir}/bin/androiddeployqt $BUILD_TYPE $SIGN --input android-libmobile_client.so-deployment-settings.json --output $BUILD_TARGET $*

cp $SOURCE_APK $TARGET_APK
