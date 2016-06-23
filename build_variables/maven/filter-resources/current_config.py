# -*- coding: utf-8 -*-
# Some build dependent variables, usable for auto.py and other python scripts 
from os import pathsep as _pathsep, name as _os_name
from os.path import abspath as _abspath
from sys import platform as _platform

ARCH="${arch}"
BOX="${box}"
CONFIGURATION="${build.configuration}"
TARGET_DIR=_abspath("${libdir}")
QT_DIR=_abspath("${qt.dir}")
QT_LIB=_abspath("${qt.dir}/lib")
# for win - add QT_LIB to PATH
# for linux - add QT_LIB to LD_LIBRARY_PATH

LIB_PATH=_abspath("${libdir}/lib/${build.configuration}")

if _os_name == 'nt':
    BIN_PATH=_abspath("${libdir}/${arch}/bin/${build.configuration}")
else:
    BIN_PATH=_abspath("${libdir}/bin/${build.configuration}")

def _add_path(env, var, paths):
    if env.get(var,'') not in ('', None):
        old = env[var].split(_pathsep)
        for path in paths.split(_pathsep):
            if path not in old:
                env[var] = _pathsep.join((env[var], path))
    else:
        env[var] = paths

def add_lib_path(env):
    if _platform == 'darwin': # Max OS X
        _add_path(env, 'DYLD_LIBRARY_PATH', LIB_PATH)
        _add_path(env, 'DYLD_FRAMEWORK_PATH', LIB_PATH)
    else:
        _add_path(env, 'LD_LIBRARY_PATH', LIB_PATH)

