include(default)
include(common/common.profile)

[settings]
os=Macos
os.version=11.0
arch=armv8
compiler=apple-clang
compiler.libcxx=libc++

build_type=Release

[env]
libvpx:CC=clang
libvpx:CXX=clang++
libvpx:CFLAGS=
libvpx:CXXFLAGS=
libvpx:LDFLAGS=
CFLAGS=-target arm64-apple-macos11
CXXFLAGS=-target arm64-apple-macos11
LDFLAGS=-target arm64-apple-macos11
