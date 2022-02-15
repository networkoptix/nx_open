include(default)
include(common.profile)

[settings]
os=Macos
compiler=apple-clang
compiler.libcxx=libc++
compiler.cppstd=20

[env]
libvpx:CC=clang
libvpx:CXX=clang++
