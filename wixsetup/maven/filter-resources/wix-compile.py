import os, sys, subprocess, shutil
from subprocess import Popen, PIPE
from os.path import dirname, join, exists, isfile


bin_source_dir = '${libdir}/${arch}/bin/${build.configuration}'
server_msi_folder = 'bin'
client_msi_folder = 'bin'

server_msi_name = '${finalName}-server-only.msi'
client_msi_name = '${finalName}-client-only.msi'

commands = [\
'candle -dinstalltype="client-only" -dDbSync22SourceDir="${DbSync22Dir}" -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\\${build.configuration}-server-only\\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll Associations.wxs MyExitDialog.wxs PasswordShouldMatchDlg.wxs UpgradeDlg.wxs EmptyPasswordDlg.wxs AllFieldsAreMandatoryDlg.wxs InstallTypeDlg.wxs PortDuplicateDlg.wxs PortIsBusyDlg.wxs InvalidPasswordDlg.wxs MediaServerDlg.wxs ClientDlg.wxs SelectionWarning.wxs Product-client-only.wxs ClientHelp.wxs ClientVox.wxs ClientBg.wxs Client.wxs Server.wxs traytool.wxs DbSync22Files.wxs',\
'light -cultures:${installer.language} -cc ${project.build.directory} -reusecab -loc CustomStrings_${installer.language}.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out %s/%s -pdbout bin\\${build.configuration}\\EVEMediaPlayerSetup.wixpdb obj\\${build.configuration}-server-only\\*.wixobj' % (client_msi_folder, client_msi_name),\
'cscript FixExitDialog.js %s/%s' % (client_msi_folder, client_msi_name),\
'candle -ext WixBalExtension -dinstalltype="server-only" -dDbSync22SourceDir="${DbSync22Dir}" -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\\${build.configuration}-client-only\\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll UninstallOptionsDlg.wxs Associations.wxs MyExitDialog.wxs PasswordShouldMatchDlg.wxs UpgradeDlg.wxs EmptyPasswordDlg.wxs AllFieldsAreMandatoryDlg.wxs InstallTypeDlg.wxs PortDuplicateDlg.wxs PortIsBusyDlg.wxs InvalidPasswordDlg.wxs MediaServerDlg.wxs ClientDlg.wxs SelectionWarning.wxs Product-server-only.wxs ClientHelp.wxs ClientVox.wxs ClientBg.wxs Client.wxs Server.wxs traytool.wxs DbSync22Files.wxs',\
'light -ext WixBalExtension -cultures:${installer.language} -cc ${project.build.directory} -reusecab -loc CustomStrings_${installer.language}.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out %s/%s -pdbout bin\\${build.configuration}\\EVEMediaPlayerSetup.wixpdb obj\\${build.configuration}-client-only\\*.wixobj' % (server_msi_folder, server_msi_name),\
'cscript FixExitDialog.js %s/%s' % (server_msi_folder, server_msi_name)
]

for command in commands:
    p = subprocess.Popen(command, shell=True, stdout=PIPE)
    out, err = p.communicate()
    print ('\n++++++++++++++++++++++%s++++++++++++++++++++++' % command)
    print out
    p.wait()
    if p.returncode != 0 and p.returncode != 204:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)

client_msi_product_code = subprocess.check_output('cscript //NoLogo productcode.js %s\\%s' % (client_msi_folder, client_msi_name)).strip()
server_msi_product_code = subprocess.check_output('cscript //NoLogo productcode.js %s\\%s' % (server_msi_folder, server_msi_name)).strip()

assert(len(client_msi_product_code) > 0)
assert(len(server_msi_product_code) > 0)
	
with open('generated_variables.iss', 'w') as f:
    print >> f, '#define ServerMsiFolder "%s"' % server_msi_folder
    print >> f, '#define ClientMsiFolder "%s"' % client_msi_folder
    print >> f, '#define ServerMsiName "%s"' % server_msi_name
    print >> f, '#define ClientMsiName "%s"' % client_msi_name
    print >> f, '#define ServerMsiProductCode "%s"' % server_msi_product_code
    print >> f, '#define ClientMsiProductCode "%s"' % client_msi_product_code

command = os.path.join(os.getenv('environment'), "is5/ISCC.exe") + ' /obin /f${finalName} Product-full.iss'
p = subprocess.Popen(command, shell=True, stdout=PIPE)
out, err = p.communicate()
print ('\n++++++++++++++++++++++%s++++++++++++++++++++++' % command)
print out
p.wait()
if p.returncode != 0 and p.returncode != 204:  
    print "failed with code: %s" % str(p.returncode) 
    sys.exit(1)

if os.path.exists(join(bin_source_dir, '${product.name} Launcher.exe')):
    os.unlink(join(bin_source_dir, '${product.name} Launcher.exe'))
if os.path.exists(join(bin_source_dir, 'applauncher.exe')):
    shutil.copy2(join(bin_source_dir, 'applauncher.exe'), join(bin_source_dir, '${product.name} Launcher.exe'))
    
if os.path.exists(join(bin_source_dir, '${product.name}.exe')):
    os.unlink(join(bin_source_dir, '${product.name}.exe'))          
if os.path.exists(join(bin_source_dir, 'client.exe')):
    shutil.copy2(join(bin_source_dir, 'client.exe'), join(bin_source_dir, '${product.name}.exe'))