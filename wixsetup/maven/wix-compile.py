import os, sys, subprocess

try:
    os.system('candle -dinstalltype="all" -dAppServerSourceDir="${AppServerDir}" -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\${build.configuration}\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll UninstallOptionsDlg.wxs Associations.wxs MyExitDialog.wxs PasswordShouldMatchDlg.wxs UpgradeDlg.wxs PasswordDlg.wxs EmptyPasswordDlg.wxs AllFieldsAreMandatoryDlg.wxs InstallTypeDlg.wxs PortDuplicateDlg.wxs PortIsBusyDlg.wxs InvalidPasswordDlg.wxs MediaDirDlg.wxs MediaServerDlg.wxs ClientDlg.wxs MyFeaturesDlg.wxs SelectionWarning.wxs DowngradeWarningDlg.wxs Product-all.wxs AppServerFiles.wxs ClientHelp.wxs ClientVox.wxs ClientBg.wxs AppServerDlg.wxs Client.wxs')
    #subprocess.call(['candle -dAppServerSourceDir="${AppServerDir}" -dClientHelpSourceDir="${ClientHelpSourceDir} -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\${build.configuration}\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll UninstallOptionsDlg.wxs Associations.wxs MyExitDialog.wxs PasswordShouldMatchDlg.wxs UpgradeDlg.wxs PasswordDlg.wxs EmptyPasswordDlg.wxs AllFieldsAreMandatoryDlg.wxs InstallTypeDlg.wxs PortDuplicateDlg.wxs PortIsBusyDlg.wxs InvalidPasswordDlg.wxs MediaDirDlg.wxs MediaServerDlg.wxs ClientDlg.wxs MyFeaturesDlg.wxs SelectionWarning.wxs DowngradeWarningDlg.wxs Product-all.wxs AppServerFiles.wxs ClientHelp.wxs ClientVox.wxs ClientBg.wxs AppServerDlg.wxs Client.wxs'])
except OSError:
    print ('++++++++++++++++++++++candle failure++++++++++++++++++++++')
    sys.exit(1)

try:
    os.system('light -cultures:en-US -loc CustomStrings.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out bin/${project.build.finalName}-full.msi -pdbout bin\${build.configuration}\EVEMediaPlayerSetup.wixpdb obj\${build.configuration}\*.wixobj"')
    #subprocess.call(['light -cultures:en-US -loc CustomStrings.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out bin/${project.build.finalName}-full.msi -pdbout bin\${build.configuration}\EVEMediaPlayerSetup.wixpdb obj\${build.configuration}\*.wixobj"'])
except OSError:
    print ('++++++++++++++++++++++light failure++++++++++++++++++++++')    
    sys.exit(1)

try:
    os.system('cscript FixExitDialog.js bin/${project.build.finalName}-full.msi')
    #subprocess.call(['cscript FixExitDialog.js bin/${project.build.finalName}-full.msi'])
except OSError:
    print ('++++++++++++++++++++++cscript failure++++++++++++++++++++++')        
    sys.exit(1)
    
try:
    os.system('candle -dinstalltype="client" -dAppServerSourceDir="${AppServerDir}" -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\${build.configuration}\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll UninstallOptionsDlg.wxs Associations.wxs MyExitDialog.wxs PasswordShouldMatchDlg.wxs UpgradeDlg.wxs PasswordDlg.wxs EmptyPasswordDlg.wxs AllFieldsAreMandatoryDlg.wxs InstallTypeDlg.wxs PortDuplicateDlg.wxs PortIsBusyDlg.wxs InvalidPasswordDlg.wxs MediaDirDlg.wxs MediaServerDlg.wxs ClientDlg.wxs MyFeaturesDlg.wxs SelectionWarning.wxs DowngradeWarningDlg.wxs Product-client.wxs AppServerFiles.wxs ClientHelp.wxs ClientVox.wxs ClientBg.wxs AppServerDlg.wxs Client.wxs')
    #subprocess.call(['candle -dAppServerSourceDir="${AppServerDir}" -dClientHelpSourceDir="${ClientHelpSourceDir} -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\${build.configuration}\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll UninstallOptionsDlg.wxs Associations.wxs MyExitDialog.wxs PasswordShouldMatchDlg.wxs UpgradeDlg.wxs PasswordDlg.wxs EmptyPasswordDlg.wxs AllFieldsAreMandatoryDlg.wxs InstallTypeDlg.wxs PortDuplicateDlg.wxs PortIsBusyDlg.wxs InvalidPasswordDlg.wxs MediaDirDlg.wxs MediaServerDlg.wxs ClientDlg.wxs MyFeaturesDlg.wxs SelectionWarning.wxs DowngradeWarningDlg.wxs Product-client.wxs AppServerFiles.wxs ClientHelp.wxs ClientVox.wxs ClientBg.wxs AppServerDlg.wxs Client.wxs'])
except OSError:
    print ('++++++++++++++++++++++candle failure++++++++++++++++++++++')
    sys.exit(1)

try:
    os.system('light -cultures:en-US -loc CustomStrings.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out bin/${project.build.finalName}-client.msi -pdbout bin\${build.configuration}\EVEMediaPlayerSetup.wixpdb obj\${build.configuration}\*.wixobj"')
    #subprocess.call(['light -cultures:en-US -loc CustomStrings.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out bin/${project.build.finalName}-client.msi -pdbout bin\${build.configuration}\EVEMediaPlayerSetup.wixpdb obj\${build.configuration}\*.wixobj"'])
except OSError:
    print ('++++++++++++++++++++++light failure++++++++++++++++++++++')    
    sys.exit(1)

try:
    os.system('cscript FixExitDialog.js bin/${project.build.finalName}-client.msi')
    #subprocess.call(['cscript FixExitDialog.js bin/${project.build.finalName}-client.msi'])
except OSError:
    print ('++++++++++++++++++++++cscript failure++++++++++++++++++++++')        
    sys.exit(1)



