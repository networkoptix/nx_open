include(default)
include(common.profile)

[settings]
os=Windows
compiler=Visual Studio
compiler.version=16
compiler.cppstd=20

[options]
libvpx:shared=False

[build_requires]
openssl*:jom/1.1.2
qt*:patch-windows/0.1,strawberryperl/5.30.0.1
