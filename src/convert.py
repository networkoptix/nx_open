#!/usr/bin/env python

import shutil
from version import *
from genskin import genskin
import os

EXCLUDE_DIRS = ('.svn', 'dxva')
EXCLUDE_FILES = ('dxva', 'moc_', 'qrc_', 'StdAfx')

def is_exclude_file(f):
    for exclude_file in EXCLUDE_FILES:
        if exclude_file in f:
            return True

    return False

try:
    shutil.rmtree('release')
    shutil.rmtree('debug')
except:
    pass

version_h = open('version.h', 'w')
print >> version_h, '#ifndef UNIVERSAL_CLIENT_VERSION_H_'
print >> version_h, '#define UNIVERSAL_CLIENT_VERSION_H_'
print >> version_h, '// This file is generated. Go to version.py'
print >> version_h, 'static const char* ORGANIZATION_NAME="%s";' % ORGANIZATION_NAME
print >> version_h, 'static const char* APPLICATION_NAME="%s";' % APPLICATION_NAME
print >> version_h, 'static const char* APPLICATION_VERSION="%s";' % APPLICATION_VERSION
print >> version_h, '#endif // UNIVERSAL_CLIENT_VERSION_H_'

genskin()

shutil.copy('const.pro', 'uniclient.pro')

headers = []
sources = []

for root, dirs, files in os.walk('.'):
    parent = root[2:]
    for exclude_dir in EXCLUDE_DIRS:
        if exclude_dir in dirs:
            dirs.remove(exclude_dir)    # don't visit SVN directories

    for f in files:
        if is_exclude_file(f):
            continue

        if f.endswith('.h'):
            headers.append(os.path.join(parent, f))
        elif f.endswith('.cpp'):
            sources.append(os.path.join(parent, f))

uniclient_pro = open('uniclient.pro', 'a')

for header in headers:
    print >> uniclient_pro, "HEADERS += %s" % header

for cpp in sources:
    print >> uniclient_pro,    "SOURCES += %s" % cpp

uniclient_pro.close()
