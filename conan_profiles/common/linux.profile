include(default)
include(common.profile)

[settings]
os=Linux
compiler=gcc
compiler.version=12
compiler.libcxx=libstdc++11
compiler.cppstd=20

[options]
libmysqlclient/*:shared=True
libpq/*:shared=True
qt/*:os_deps_package=os_deps_for_desktop_linux/ubuntu_bionic
opencv/*:fPIC=True
opencv/*:with_gtk=False
opencv/*:with_cufft=False
opencv/*:with_v4l=False

[tool_requires]
gcc-toolchain/12.2
