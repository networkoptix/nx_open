# To run this script install WiX 3.5 from wix.sourceforge.net and add its bin dir to system path

import os, sys

sys.path.append('..')

from version import *
from genassoc import gen_assoc, gen_file_exts

os.putenv('ORGANIZATION_NAME', ORGANIZATION_NAME)
os.putenv('APPLICATION_NAME', APPLICATION_NAME)
os.putenv('APPLICATION_VERSION', APPLICATION_VERSION)

# Generate Associations.wxs
gen_assoc()

# Generate FileExtensions.wxs
gen_file_exts()

os.system(r'candle -dORGANIZATION_NAME="%s" -dAPPLICATION_NAME="%s" -dAPPLICATION_VERSION="%s" -out obj\Release\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext WixSystemToolsExtension.dll *.wxs' % (ORGANIZATION_NAME, APPLICATION_NAME, APPLICATION_VERSION))

output_file = 'bin/Release/EVEMediaPlayerSetup-%s.msi' % APPLICATION_VERSION
try:
    os.unlink(output_file)
except OSError:
    pass

os.system(r'light -cultures:en-US -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext WixSystemToolsExtension.dll -out %s -pdbout bin\Release\EVEMediaPlayerSetup.wixpdb obj\Release\*.wixobj' % output_file)

# os.system(r'cscript ModifyMsiToAllowRegistryRestore.js %s' % output_file)
os.system(r'cscript FixExitDialog.js %s' % output_file)

