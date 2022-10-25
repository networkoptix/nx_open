include(common/linux.profile)

TOOLCHAIN_PREFIX=arm-linux-gnueabihf

[settings]
arch=armv7hf
libvpx:arch=armv7

[options]
gcc-toolchain:glibc_version=2.24
gcc-toolchain:linux_version=4.9

[env]
TOOLCHAIN_PREFIX=$TOOLCHAIN_PREFIX
CC=$TOOLCHAIN_PREFIX-gcc
CXX=$TOOLCHAIN_PREFIX-g++
RANLIB=$TOOLCHAIN_PREFIX-ranlib
AS=$TOOLCHAIN_PREFIX-as
STRIP=$TOOLCHAIN_PREFIX-strip
