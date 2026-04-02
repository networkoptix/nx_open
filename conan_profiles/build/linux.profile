[settings]
os=Linux
arch=x86_64
build_type=Release
compiler=gcc
compiler.version=13
compiler.libcxx=libstdc++11
compiler.cppstd=gnu17

ninja/*:compiler.libcxx=libstdc++11

[options]
cpython/*:shared = False
cpython/*:optimizations = True
cpython/*:docstrings = False
cpython/*:with_curses = False
cpython/*:with_gdbm = False
cpython/*:with_sqlite3 = False
cpython/*:with_tkinter = False
cpython/*:with_bz2 = False
cpython/*:with_lzma = False

[tool_requires]
!(gcc-toolchain/*|crosstool-ng/*|qt-host/*): gcc-toolchain/13.3

[buildenv]
LDFLAGS+=-static-libstdc++ -static-libgcc
