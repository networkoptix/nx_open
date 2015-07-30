#!/bin/bash

BUILD_TARGET=apk
ANDROID_NDK_ROOT=${env.environment}/android/android-ndk

if [ -d "$BUILD_TARGET/libs" ]; then
    rm -rf $BUILD_TARGET/libs
fi

BUILD_TYPE=
SIGN=
if [[ ${build.configuration} == "release" ]]; then
    BUILD_TYPE=--release

    if [[ ${skip.sign} != true ]]; then
        SIGN="--sign ${google.keystore} ${google.alias} --storepass ${google.storepass} --keypass ${google.keypass}"
    fi
fi


make install --makefile=Makefile.${build.configuration} INSTALL_ROOT=$BUILD_TARGET
${libdir}/bin/androiddeployqt $BUILD_TYPE $SIGN --input android-deployment.json --output $BUILD_TARGET $*
