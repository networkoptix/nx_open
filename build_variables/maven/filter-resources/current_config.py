# -*- coding: utf-8 -*-
# Some build dependent variables, usable for auto.py and other python scripts 
ARCH="${arch}"
BOX="${box}"
CONFIGURATION="${build.configuration}"
TARGET_DIR="${libdir}"
QT_DIR="${qt.dir}"
QT_LIB="${qt.dir}/lib"
# for win - add QT_LIB to PATH
# for linux - add QT_LIB to LD_LIBRARY_PATH

LIB_PATH="${libdir}/lib/${build.configuration}"

from os import pathsep as _pathsep, name as _os_name
from sys import platform as _platform

if _os_name == 'nt':
    BIN_PATH="${libdir}/${arch}/bin/${build.configuration}"
else:
    BIN_PATH="${libdir}/bin/${build.configuration}"

def _add_path(env, var, path):
    if env.get(var,'') not in ('', None):
        env[var] += _pathsep + path
    else:
        env[var] = path

def add_lib_path(env):
    if _platform == 'darwin': # Max OS X
        _add_path(Env, 'DYLD_LIBRARY_PATH', LIB_PATH)
        _add_path(Env, 'DYLD_FRAMEWORK_PATH', LIB_PATH)
    else:
        _add_path(Env, 'LD_LIBRARY_PATH', LIB_PATH)

