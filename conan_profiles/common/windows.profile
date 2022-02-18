include(default)
include(common.profile)

[settings]
os=Windows
compiler=Visual Studio
compiler.version=16
build_type=Release

[options]
libvpx:shared=False

[build_requires]
openssl*:jom/1.1.2
qt-*:jom/1.1.2,patch-windows/0.1
