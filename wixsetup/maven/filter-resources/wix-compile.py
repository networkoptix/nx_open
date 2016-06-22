import os, sys, subprocess, shutil
from subprocess import Popen, PIPE
from os.path import dirname, join, exists, isfile


bin_source_dir = '${libdir}/${arch}/bin/${build.configuration}'
server_msi_folder = 'bin/msi'
client_msi_folder = 'bin/msi'
nxtool_msi_folder = 'bin/msi'
server_msi_strip_folder = 'bin/strip'
client_msi_strip_folder = 'bin/strip'

server_msi_name = '${finalName}-server-only.msi'
client_msi_name = '${finalName}-client-only.msi'
nxtool_msi_name = '${finalName}-servertool.msi'

commands = [\
'candle -dinstalltype="client-only" -dVs2015crtDir="${Vs2015crtDir}"  -dClientQmlDir="${ClientQmlDir}" -dDbSync22SourceDir="${DbSync22Dir}" -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientFontsDir="${ClientFontsDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\\${build.configuration}-client-only\\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll Associations.wxs MyExitDialog.wxs UpgradeDlg.wxs ClientDlg.wxs SelectionWarning.wxs Product-client-only.wxs ClientHelp.wxs ClientFonts.wxs ClientVox.wxs ClientBg.wxs Client.wxs ClientQml.wxs vs2015crt.wxs',\
'light -cultures:${installer.language} -cc ${libdir}/${arch}/bin/${build.configuration}/cab -reusecab -loc CustomStrings_${installer.language}.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out %s/%s -pdbout %s\\${build.configuration}\\EVEMediaPlayerSetup.wixpdb obj\\${build.configuration}-client-only\\*.wixobj' % (client_msi_folder, client_msi_name, client_msi_folder),\
'cscript FixExitDialog.js %s/%s' % (client_msi_folder, client_msi_name),\
'candle -ext WixBalExtension -dinstalltype="server-only" -dVs2015crtDir="${Vs2015crtDir}"  -dDbSync22SourceDir="${DbSync22Dir}" -dClientQmlDir="${ClientQmlDir}"  -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientFontsDir="${ClientFontsDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\\${build.configuration}-server-only\\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll UninstallOptionsDlg.wxs MyExitDialog.wxs UpgradeDlg.wxs EmptyPasswordDlg.wxs MediaServerDlg.wxs SelectionWarning.wxs Product-server-only.wxs ServerVox.wxs Server.wxs traytool.wxs DbSync22Files.wxs vs2015crt.wxs',\
'light -ext WixBalExtension -cultures:${installer.language} -cc ${libdir}/${arch}/bin/${build.configuration}/cab -reusecab -loc CustomStrings_${installer.language}.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out %s/%s -pdbout %s\\${build.configuration}\\EVEMediaPlayerSetup.wixpdb obj\\${build.configuration}-server-only\\*.wixobj' % (server_msi_folder, server_msi_name, server_msi_folder),\
'cscript FixExitDialog.js %s/%s' % (server_msi_folder, server_msi_name), \
'candle -dinstalltype="client-only" -dVs2015crtDir="${Vs2015crtDir}"  -dClientQmlDir="${ClientQmlDir}" -dDbSync22SourceDir="${DbSync22Dir}" -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientFontsDir="${ClientFontsDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\\${build.configuration}-client-strip\\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll Associations.wxs MyExitDialog.wxs UpgradeDlg.wxs ClientDlg.wxs SelectionWarning.wxs Product-client-strip.wxs ClientHelp.wxs ClientFonts.wxs ClientVox.wxs ClientBg.wxs Client.wxs ClientQml.wxs vs2015crt.wxs',\
'light -cultures:${installer.language} -cc ${libdir}/${arch}/bin/${build.configuration}/cab -reusecab -loc CustomStrings_${installer.language}.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out %s/%s -pdbout %s\\${build.configuration}\\EVEMediaPlayerSetup.wixpdb obj\\${build.configuration}-client-strip\\*.wixobj' % (client_msi_strip_folder, client_msi_name, client_msi_strip_folder),\
'cscript FixExitDialog.js %s/%s' % (client_msi_strip_folder, client_msi_name),\
'candle -ext WixBalExtension -dinstalltype="server-only" -dVs2015crtDir="${Vs2015crtDir}"  -dDbSync22SourceDir="${DbSync22Dir}" -dClientQmlDir="${ClientQmlDir}"  -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientFontsDir="${ClientFontsDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\\${build.configuration}-server-strip\\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll UninstallOptionsDlg.wxs MyExitDialog.wxs UpgradeDlg.wxs EmptyPasswordDlg.wxs MediaServerDlg.wxs SelectionWarning.wxs Product-server-strip.wxs ServerVox.wxs Server.wxs traytool.wxs DbSync22Files.wxs vs2015crt.wxs',\
'light -ext WixBalExtension -cultures:${installer.language} -cc ${libdir}/${arch}/bin/${build.configuration}/cab -reusecab -loc CustomStrings_${installer.language}.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out %s/%s -pdbout %s\\${build.configuration}\\EVEMediaPlayerSetup.wixpdb obj\\${build.configuration}-server-strip\\*.wixobj' % (server_msi_strip_folder, server_msi_name, server_msi_strip_folder),\
'cscript FixExitDialog.js %s/%s' % (server_msi_strip_folder, server_msi_name)
]
if '${nxtool}' == 'true':
    commands += [\
    'candle -dinstalltype="client-only" -dVs2015crtDir="${Vs2015crtDir}" -dNxtoolQuickControlsDir="${NxtoolQuickControlsDir}" -dNxtoolQmlDir="${project.build.directory}\\nxtoolqml" -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\\${build.configuration}-nxtool\\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll MyExitDialog.wxs UpgradeDlg.wxs NxtoolDlg.wxs SelectionWarning.wxs Product-nxtool.wxs Nxtool.wxs NxtoolQuickControls.wxs vs2015crt.wxs',\
    # Using windeployqt
    #'candle -dinstalltype="client-only" -dVs2015crtDir="${Vs2015crtDir}" -dNxtoolQuickControlsDir="${NxtoolQuickControlsDir}" -dNxtoolQmlDir="${project.build.directory}\\nxtoolqml" -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\\${build.configuration}-nxtool\\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll MyExitDialog.wxs UpgradeDlg.wxs NxtoolDlg.wxs SelectionWarning.wxs Product-nxtool.wxs Nxtool.wxs NxtoolQml.wxs vs2015crt.wxs',\
    'light -cultures:${installer.language} -cc ${libdir}/${arch}/bin/${build.configuration}/cab -reusecab -loc CustomStrings_${installer.language}.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out %s/%s -pdbout %s\\${build.configuration}\\EVEMediaPlayerSetup.wixpdb obj\\${build.configuration}-nxtool\\*.wixobj' % (nxtool_msi_folder, nxtool_msi_name, nxtool_msi_folder),\
    'cscript FixExitDialog.js %s/%s' % (nxtool_msi_folder, nxtool_msi_name)
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

client_msi_product_code = subprocess.check_output('cscript //NoLogo productcode.js %s\\%s' % (client_msi_strip_folder, client_msi_name)).strip()
server_msi_product_code = subprocess.check_output('cscript //NoLogo productcode.js %s\\%s' % (server_msi_strip_folder, server_msi_name)).strip()

assert(len(client_msi_product_code) > 0)
assert(len(server_msi_product_code) > 0)
	
with open('generated_variables.iss', 'w') as f:
    print >> f, '#define ServerMsiFolder "%s"' % server_msi_strip_folder
    print >> f, '#define ClientMsiFolder "%s"' % client_msi_strip_folder
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
if os.path.exists(join(bin_source_dir, 'client.bin.exe')):
    shutil.copy2(join(bin_source_dir, 'client.bin.exe'), join(bin_source_dir, '${product.name}.exe'))
