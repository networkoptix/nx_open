include(common/linux.profile)

[settings]
arch=armv7hf
libvpx/*:arch=armv7

[options]
gcc-toolchain/*:glibc_version=2.24
gcc-toolchain/*:linux_version=4.9
