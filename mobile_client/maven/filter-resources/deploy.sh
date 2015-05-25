#!/bin/ssh

BUILD_TARGET=apk
ANDROID_NDK_ROOT=${env.environment}/android/android-ndk

if [ -d "$BUILD_TARGET/libs" ]; then
    rm -rf $BUILD_TARGET/libs
fi

make install INSTALL_ROOT=$BUILD_TARGET
${libdir}/bin/androiddeployqt --input android-deployment.json --output $BUILD_TARGET $*
