# To run this script install WiX 3.5 from wix.sourceforge.net and add its bin dir to system path

import os, sys

sys.path.append('..')

from version import *
# from genassoc import gen_assoc, gen_file_exts
from string import Template
from fixasfiles import fixasfiles

from common.convert import rmtree

set_env()

if len(sys.argv) == 2 and sys.argv[1].lower() == 'debug':
    CONFIG = 'Debug'
else:
    CONFIG = 'Release'

os.environ['CONFIG'] = CONFIG    

if os.path.exists(os.path.join('bin', CONFIG)):
    rmtree(os.path.join('bin', CONFIG))

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

WXS_FILES = "InstallTypeDlg.wxs AdvancedTypeDlg.wxs PortDuplicateDlg.wxs PortIsBusyDlg.wxs MediaDirDlg.wxs MediaServerDlg.wxs MyFeaturesDlg.wxs SelectionWarning.wxs DowngradeWarningDlg.wxs  ClientDlg.wxs  Product.wxs AppServerFiles.wxs AppServerDlg.wxs FileAssociations.wxs"
os.system(r'heat dir ..\appserver\setup\build\exe.win32-2.7 -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out AppServerFiles.wxs -cg AppServerFilesComponent -dr VmsAppServerDir -var var.AppServerSourceDir -wixvar')
fixasfiles()

os.system(r'candle -dAppServerSourceDir="../appserver/setup/build/exe.win32-2.7" -out obj\%s\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll %s' % (CONFIG, WXS_FILES))

if CONFIG == 'Debug':
    output_file = 'bin/HDWitness-%s.%s-Debug.msi' % (APPLICATION_VERSION, BUILD_NUMBER)
else:
    output_file = 'bin/HDWitness-%s.%s.msi' % (APPLICATION_VERSION, BUILD_NUMBER)
try:
    os.unlink(output_file)
except OSError:
    pass

os.system(r'light -cultures:en-US -loc CustomStrings.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -out %s -pdbout bin\%s\EVEMediaPlayerSetup.wixpdb obj\%s\*.wixobj' % (output_file, CONFIG, CONFIG))

os.system(r'cscript FixExitDialog.js %s' % output_file)


if not os.path.exists(output_file):
    print >> sys.stderr, 'Output file is not created. Build failed'
    sys.exit(1)