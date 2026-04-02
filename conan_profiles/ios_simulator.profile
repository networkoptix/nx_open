include(common/ios.profile)

[settings]
arch=armv8
os.sdk=iphonesimulator

[buildenv]
COMMON_TARGET=-target arm64-apple-ios-simulator
SYSROOT=-isysroot $(xcrun --sdk iphonesimulator --show-sdk-path)

CFLAGS+=$COMMON_TARGET $SYSROOT
CXXFLAGS+=$COMMON_TARGET $SYSROOT
OBJCFLAGS+=$COMMON_TARGET $SYSROOT
OBJCXXFLAGS+=$COMMON_TARGET $SYSROOT
LDFLAGS+=$COMMON_TARGET $SYSROOT
