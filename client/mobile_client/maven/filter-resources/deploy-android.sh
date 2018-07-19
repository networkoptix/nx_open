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
    SIGN="--sign ${android.keystore} ${android.alias} --storepass ${android.storepass} --keypass ${android.keypass}"
    SOURCE_APK=apk/bin/QtApp-release-signed.apk
    if [[ -z "${android.storepass}" ]]; then
        SIGN=
        SOURCE_APK=apk/bin/QtApp-release-unsigned.apk
    fi
else
    SOURCE_APK=apk/bin/QtApp-debug.apk
fi
TARGET_APK=${client_distribution_name}.apk

rm -rf $BUILD_TARGET

set -e

make install --makefile=Makefile.${build.configuration} INSTALL_ROOT=$BUILD_TARGET
${qt.dir}/bin/androiddeployqt $BUILD_TYPE $SIGN --input android-libmobile_client.so-deployment-settings.json --output $BUILD_TARGET $*

mv $SOURCE_APK $TARGET_APK
