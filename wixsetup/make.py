# To run this script install WiX 3.5 from wix.sourceforge.net and add its bin dir to system path

import os, sys

sys.path.append('..')

from version import *
# from genassoc import gen_assoc, gen_file_exts
from string import Template

def gen_strings():
    xin = open('CustomStrings.wxl.template', 'r').read()
    xout = open('CustomStrings.wxl', 'w')

    strings_template = Template(xin)
    print >> xout, strings_template.substitute(APPLICATION_VERSION=APPLICATION_VERSION)

os.putenv('ORGANIZATION_NAME', ORGANIZATION_NAME)
os.putenv('APPLICATION_NAME', APPLICATION_NAME)
os.putenv('APPLICATION_VERSION', APPLICATION_VERSION)

# Generate Associations.wxs
# gen_assoc()

# Generate FileExtensions.wxs
# gen_file_exts()

# Generate CustomStrings.wxl
gen_strings()

os.system(r'candle -dORGANIZATION_NAME="%s" -dAPPLICATION_NAME="%s" -dAPPLICATION_VERSION="%s" -out obj\Release\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll *.wxs' % (ORGANIZATION_NAME, APPLICATION_NAME, APPLICATION_VERSION))

output_file = 'bin/VMS-%s.msi' % APPLICATION_VERSION
try:
    os.unlink(output_file)
except OSError:
    pass

os.system(r'light -cultures:en-US -loc CustomStrings.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -out %s -pdbout bin\Release\EVEMediaPlayerSetup.wixpdb obj\Release\*.wixobj' % output_file)

os.system(r'cscript FixExitDialog.js %s' % output_file)

