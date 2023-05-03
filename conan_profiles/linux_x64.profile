include(common/linux.profile)

TOOLCHAIN_PREFIX=x86_64-linux-gnu

[settings]
arch=x86_64

[options]
opencv:with_cuda=True
opencv:cuda_arch_bin=5.0
qt:mysql=True
qt:psql=True

[env]
TOOLCHAIN_PREFIX=$TOOLCHAIN_PREFIX
CC=$TOOLCHAIN_PREFIX-gcc
CXX=$TOOLCHAIN_PREFIX-g++
RANLIB=$TOOLCHAIN_PREFIX-ranlib
AS=$TOOLCHAIN_PREFIX-as
STRIP=$TOOLCHAIN_PREFIX-strip

[build_requires]
opencv*:cuda-toolkit/11.7
