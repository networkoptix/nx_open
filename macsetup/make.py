#!/usr/bin/env python

import os, sys, shutil

sys.path.append('..')

from version import *

destination = 'EvePlayer-Beta-%s.dmg' % APPLICATION_VERSION
os.unlink(destination)

pwd = os.getcwd()
os.chdir('../src')
os.system("xcodebuild -configuration Release")
os.chdir(pwd)

os.chdir('../bin/release')
os.system('macdeployqt EvePlayer-Beta.app -dmg')
os.chdir(pwd)

shutil.move('../bin/release/EvePlayer-Beta.dmg', destination)
