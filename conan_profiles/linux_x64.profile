include(common/linux.profile)

TOOLCHAIN_PREFIX=x86_64-linux-gnu

[settings]
arch=x86_64

[options]
qt:mysql=True
qt:psql=True

[env]
TOOLCHAIN_PREFIX=$TOOLCHAIN_PREFIX
CC=$TOOLCHAIN_PREFIX-gcc
CXX=$TOOLCHAIN_PREFIX-g++
RANLIB=$TOOLCHAIN_PREFIX-ranlib
AS=$TOOLCHAIN_PREFIX-as
STRIP=$TOOLCHAIN_PREFIX-strip
