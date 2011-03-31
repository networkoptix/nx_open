# To run this script install WiX 3.5 from wix.sourceforge.net and add its bin dir to system path

import os, sys

sys.path.append('..')

from version import *

os.putenv('ORGANIZATION_NAME', ORGANIZATION_NAME)
os.putenv('APPLICATION_NAME', APPLICATION_NAME)
os.putenv('APPLICATION_VERSION', APPLICATION_VERSION)

os.system(r'candle -dORGANIZATION_NAME="%s" -dAPPLICATION_NAME="%s" -dAPPLICATION_VERSION="%s" -out obj\Release\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll *.wxs' % (ORGANIZATION_NAME, APPLICATION_NAME, APPLICATION_VERSION))

try:
    os.unlink('bin/Release/EVEMediaPlayerSetup-%s.msi' % APPLICATION_VERSION)
except OSError:
    pass

os.system(r'light -cultures:en-US -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -out bin\Release\EVEMediaPlayerSetup-%s.msi -pdbout bin\Release\EVEMediaPlayerSetup.wixpdb obj\Release\*.wixobj' % APPLICATION_VERSION)

os.system(r'cscript FixExitDialog.js bin\Release\EVEMediaPlayerSetup-%s.msi' % APPLICATION_VERSION)
