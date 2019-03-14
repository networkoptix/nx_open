# -*- coding: utf-8 -*-
# Some build dependent variables, usable for auto.py and other python scripts 
from os import pathsep as _pathsep
from os.path import abspath as _abspath

ARCH="${arch}"
PLATFORM="${platform}"
BOX="${box}"
CONFIGURATION="${build.configuration}"
TARGET="${rdep.target}"
PACKAGES_DIR=_abspath("${packages.dir}")
TARGET_DIR=_abspath("${libdir}")
QT_DIR=_abspath("${qt.dir}")
QT_LIB=_abspath("${qt.dir}/lib")
PROJECT_SOURCE_DIR=_abspath("${PROJECT_SOURCE_DIR}")
# for win - add QT_LIB to PATH
# for linux - add QT_LIB to LD_LIBRARY_PATH

LIB_PATH=_abspath("${libdir}/lib/${build.configuration}")
BIN_PATH=_abspath("${libdir}/bin/${build.configuration}")
WIN_PATH=_abspath("${libdir}/bin/${build.configuration}")

def _add_path(env, var, paths):
    if env.get(var,'') not in ('', None):
        old = env[var].split(_pathsep)
        for path in paths.split(_pathsep):
            if path not in old:
                env[var] = _pathsep.join((env[var], path))
    else:
        env[var] = paths

def add_lib_path(env):
    target = TARGET.lower()
    if target.startswith('windows'):
         _add_path(env, 'PATH', QT_LIB)
    else:
        libs = _pathsep.join((LIB_PATH, QT_LIB))
        if target.startswith('macosx'):
            _add_path(env, 'DYLD_LIBRARY_PATH', libs)
            _add_path(env, 'DYLD_FRAMEWORK_PATH', libs)
        else:
            _add_path(env, 'LD_LIBRARY_PATH', libs)

