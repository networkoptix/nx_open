include(default)
include(common.profile)

[settings]
os=Macos
os.version=11.0
compiler=apple-clang
compiler.libcxx=libc++
compiler.cppstd=20

[env]
libvpx:CC=clang
libvpx:CXX=clang++
