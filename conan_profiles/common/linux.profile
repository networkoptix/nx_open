include(default)
include(common.profile)

[settings]
os=Linux
compiler=gcc
compiler.version=10
compiler.libcxx=libstdc++11
compiler.cppstd=20
# libmysqlclient uses C++20 keyword `requires` for variable names, also it sets -std=c++14 by
# itself.
libmysqlclient:compiler.cppstd=14

[options]
libmysqlclient:shared=True
libpq:shared=True
qt*:os_deps_package=os_deps_for_desktop_linux/ubuntu_xenial

[build_requires]
gcc-toolchain/10.2
