include(default)
include(common.profile)

[settings]
os=Linux
build_type=Release
compiler=gcc
compiler.libcxx=libstdc++11
compiler.version=10

[options]
libmysqlclient:shared=True
libpq:shared=True
qt-*:os_deps_package=os_deps_for_desktop_linux/ubuntu_xenial

[build_requires]
gcc-toolchain/10.2#53c2ddb9615885ac85d38be2ec272d8e
