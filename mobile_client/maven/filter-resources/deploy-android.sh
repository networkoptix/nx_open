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

    if [[ "${skip.sign}" != "true" ]]; then
        SIGN="--sign ${google.keystore} ${google.alias} --storepass ${google.storepass} --keypass ${google.keypass}"
        SOURCE_APK=apk/bin/QtApp-release-signed.apk
    else
        SOURCE_APK=apk/bin/QtApp-release-unsigned.apk
    fi

    if [[ "${beta}" == "true" ]]; then
        APK_SUFFIX="beta"
    else
        APK_SUFFIX="release"
    fi
else
    SOURCE_APK=apk/bin/QtApp-debug.apk
    APK_SUFFIX="debug"
fi

TARGET_APK=${product.name.short}-${project.version.name}.${project.version.code}-$APK_SUFFIX.apk

make install --makefile=Makefile.${build.configuration} INSTALL_ROOT=$BUILD_TARGET
${libdir}/bin/androiddeployqt $BUILD_TYPE $SIGN --input android-deployment.json --output $BUILD_TARGET $*

cp $SOURCE_APK $TARGET_APK
