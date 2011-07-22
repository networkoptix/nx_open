#!/usr/bin/env python

import os, sys, shutil

sys.path.append('..')

from version import *

destination = 'EvePlayer-Beta-%s.dmg' % APPLICATION_VERSION
if os.path.isfile(destination):
    os.unlink(destination)

pwd = os.getcwd()
os.chdir('../build')
os.system("make -j8 -f Makefile.release")
os.chdir(pwd)

os.chdir('../bin/release')
os.system('macdeployqt EvePlayer-Beta.app -dmg')
os.chdir(pwd)

shutil.move('../bin/release/EvePlayer-Beta.dmg', destination)
