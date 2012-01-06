# To run this script install WiX 3.5 from wix.sourceforge.net and add its bin dir to system path

import os, sys

sys.path.append('..')

from version import *
# from genassoc import gen_assoc, gen_file_exts
from string import Template
from fixasfiles import fixasfiles

from common.convert import rmtree

set_env()

if os.path.exists('bin'):
    rmtree('bin')

if os.path.exists('obj'):
    rmtree('obj')

def gen_strings():
    xin = open('CustomStrings.wxl.template', 'r').read()
    xout = open('CustomStrings.wxl', 'w')

    strings_template = Template(xin)
    print >> xout, strings_template.substitute(APPLICATION_VERSION=APPLICATION_VERSION)

# Generate Associations.wxs
# gen_assoc()

# Generate FileExtensions.wxs
# gen_file_exts()

# Generate CustomStrings.wxl
gen_strings()

WXS_FILES = "MediaDirDlg.wxs MediaServerDlg.wxs MyFeaturesDlg.wxs SelectionWarning.wxs DowngradeWarningDlg.wxs  ClientDlg.wxs  Product.wxs AppServerFiles.wxs AppServerDlg.wxs FileAssociations.wxs"
os.system(r'heat dir ..\appserver\build\exe.win32-2.7 -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out AppServerFiles.wxs -cg AppServerFilesComponent -dr VmsAppServerDir -var var.AppServerSourceDir -wixvar')
fixasfiles()

os.system(r'candle -dAppServerSourceDir="../appserver/build/exe.win32-2.7" -dORGANIZATION_NAME="%s" -dAPPLICATION_NAME="%s" -dAPPLICATION_VERSION="%s" -out obj\Release\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll %s' % (ORGANIZATION_NAME, APPLICATION_NAME, APPLICATION_VERSION, WXS_FILES))

output_file = 'bin/VMS-%s.%s.msi' % (APPLICATION_VERSION, BUILD_NUMBER)
try:
    os.unlink(output_file)
except OSError:
    pass

os.system(r'light -cultures:en-US -loc CustomStrings.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -out %s -pdbout bin\Release\EVEMediaPlayerSetup.wixpdb obj\Release\*.wixobj' % output_file)

os.system(r'cscript FixExitDialog.js %s' % output_file)


if not os.path.exists(output_file):
    print >> sys.stderr, 'Output file is not created. Build failed'
    sys.exit(1)