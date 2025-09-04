include(default)
include(common.profile)

[settings]
os=Linux
compiler=gcc
compiler.version=13
compiler.libcxx=libstdc++11
compiler.cppstd=20

# Qt since 6.8.0 requires int128 support to be enabled if the compiler supports it. But it can be
# enabled in Qt only with GNU extensions when GCC is used.
qt/*:compiler.cppstd=gnu20

[options]
cpython*:shared = False
cpython*:optimizations = True
cpython*:docstrings = False
cpython*:with_curses = False
cpython*:with_gdbm = False
icu/*:data_packaging=library
libmysqlclient/*:shared=True
libpq/*:shared=True
opencv/*:fPIC=True
opencv/*:with_gtk=False
opencv/*:with_cufft=False
opencv/*:with_v4l=False

[tool_requires]
gcc-toolchain/13.3
