# To run this script install WiX 3.5 from wix.sourceforge.net and add its bin dir to system path

import os, sys

sys.path.append('..')

from string import Template
from fixasfiles import fixasfiles

from common.convert import rmtree
from common.common_version import *
from test import *

APPLICATION_NAME = 'HD Witness'

if __name__ == '__main__':
    ORGANIZATION_NAME, APPLICATION_VERSION, BUILD_NUMBER, REVISION, FFMPEG_VERSION = set_env()
    f = open('version.bat', 'w')
    print >> f, 'SET "ORGANIZATION_NAME=%s"' % ORGANIZATION_NAME
    print >> f, 'SET "APPLICATION_NAME=%s"' % APPLICATION_NAME
    print >> f, 'SET "APPLICATION_VERSION=%s"' % APPLICATION_VERSION
    print >> f, 'SET "BUILD_NUMBER=%s"'% BUILD_NUMBER
    os.putenv('ORGANIZATION_NAME', ORGANIZATION_NAME)
    os.putenv('APPLICATION_VERSION', APPLICATION_VERSION)
    os.putenv('BUILD_NUMBER', BUILD_NUMBER)
    os.putenv('REVISION', REVISION)
    os.putenv('FFMPEG_VERSION', FFMPEG_VERSION)
    os.putenv('APPLICATION_NAME', APPLICATION_NAME)


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

# Generate CustomStrings.wxl
gen_strings()

WXS_FILES = "UpgradeDlg.wxs PasswordDlg.wxs EmptyPasswordDlg.wxs AllFieldsAreMandatoryDlg.wxs InstallTypeDlg.wxs AdvancedTypeDlg.wxs PortDuplicateDlg.wxs PortIsBusyDlg.wxs InvalidPasswordDlg.wxs MediaDirDlg.wxs MediaServerDlg.wxs MyFeaturesDlg.wxs SelectionWarning.wxs DowngradeWarningDlg.wxs Product_mvn.wxs"
# os.system(r'heat dir ..\appserver\setup\build\exe.win32-2.7 -wixvar -nologo -sfrag -suid -sreg -ag -srd -dir WebHelp -out AppServerFiles.wxs -cg AppServerFilesComponent -dr VmsAppServerDir -var var.AppServerSourceDir -wixvar')
# fixasfiles()

os.system(r'candle -dAppServerSourceDir="../appserver/setup/build/exe.win32-2.7" -out obj\%s\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll %s' % (CONFIG, WXS_FILES))

if CONFIG == 'Debug':
    output_file_base = 'bin/HDWitness-%s.%s-Debug' % (APPLICATION_VERSION, BUILD_NUMBER)
else:
    output_file_base = 'bin/HDWitness-%s.%s' % (APPLICATION_VERSION, BUILD_NUMBER)

output_file_msi = output_file_base + '.msi'
output_file_exe = output_file_base + '.exe'

try:
    os.unlink(output_file_msi)
except OSError:
    pass

try:
    os.unlink(output_file_exe)
except OSError, e:
    pass

os.system(r'light -cultures:en-US -loc CustomStrings.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -out %s -pdbout bin\%s\EVEMediaPlayerSetup.wixpdb obj\%s\*.wixobj' % (output_file_msi, CONFIG, CONFIG))

os.system(r'cscript FixExitDialog.js %s' % output_file_msi)
os.system(r'setupbld -out %s -ms %s -setup setup.exe' % (output_file_exe, output_file_msi))
# os.unlink(output_file_msi)

if not os.path.exists(output_file_exe):
    print >> sys.stderr, 'Output file is not created. Build failed'
    sys.exit(1)