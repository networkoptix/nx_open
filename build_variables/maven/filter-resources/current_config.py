# -*- coding: utf-8 -*-
# Some build dependent variables, usable for auto.py and other python scripts 
ARCH="${arch}"
BOX="${box}"
CONFIGURATION="${build.configuration}"
TARGET="${rdep.target}"
PACKAGES_DIR="${packages.dir}"
TARGET_DIR="${libdir}"
QT_DIR="${qt.dir}"
QT_LIB="${qt.dir}/lib"
# for win - add QT_LIB to PATH
# for linux - add QT_LIB to LD_LIBRARY_PATH

LIB_PATH="${libdir}/lib/${build.configuration}"
BIN_PATH="${libdir}/bin/${build.configuration}"
WIN_PATH="${libdir}/${arch}/bin/${build.configuration}"

from os import _pathsep

def _add_path(env, var, path):
    if env.get(var,'') not in ('', None):
        env[var] += _pathsep + path
    else:
        env[var] = path

def add_lib_path(env):
    target = TARGET.lower()
    if target.startswith('windows'):
         _add_path(Env, 'PATH', QT_LIB)
    else:
        libs = _pathsep.join((LIB_PATH, QT_LIB)
        if target.startswith('macosx'):
            _add_path(Env, 'DYLD_LIBRARY_PATH', libs)
            _add_path(Env, 'DYLD_FRAMEWORK_PATH', libs)
        else:
            _add_path(Env, 'LD_LIBRARY_PATH', libs)

