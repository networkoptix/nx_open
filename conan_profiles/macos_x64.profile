include(default)
include(common/common.profile)

[settings]
os=Macos
arch=x86_64
compiler=apple-clang
compiler.libcxx=libc++

build_type=Release

[env]
libvpx:CC=clang
libvpx:CXX=clang++
