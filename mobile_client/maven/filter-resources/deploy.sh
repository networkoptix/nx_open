#!/bin/ssh

BUILD_TARGET=apk
ANDROID_NDK_ROOT=${env.environment}/android/android-ndk

make install INSTALL_ROOT=$BUILD_TARGET
${libdir}/bin/androiddeployqt --input android-deployment.json --output $BUILD_TARGET
