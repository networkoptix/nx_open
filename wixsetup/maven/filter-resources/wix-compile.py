import os, sys, subprocess, shutil
from subprocess import Popen, PIPE

commands = [\
'candle -dinstalltype="${install.type}" -dDbSync22SourceDir="${DbSync22Dir}" -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\${build.configuration}\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll UninstallOptionsDlg.wxs Associations.wxs MyExitDialog.wxs PasswordShouldMatchDlg.wxs UpgradeDlg.wxs EmptyPasswordDlg.wxs AllFieldsAreMandatoryDlg.wxs InstallTypeDlg.wxs PortDuplicateDlg.wxs PortIsBusyDlg.wxs InvalidPasswordDlg.wxs MediaDirDlg.wxs MediaServerDlg.wxs ClientDlg.wxs SelectionWarning.wxs DowngradeWarningDlg.wxs Product-${install.type}.wxs ClientHelp.wxs ClientVox.wxs ClientBg.wxs Client.wxs Server.wxs traytool.wxs MyFeaturesDlg.wxs DbSync22Files.wxs',\
'light -cultures:${installer.language} -loc CustomStrings_${installer.language}.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out bin/${project.build.finalName}.msi -pdbout bin\${build.configuration}\EVEMediaPlayerSetup.wixpdb obj\${build.configuration}\*.wixobj"',\
'cscript FixExitDialog.js bin/${project.build.finalName}.msi'\
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
    
#for files in os.listdir('${project.build.directory}/bin/'):
#    if files.endswith(".msi"):
#        shutil.move(os.path.join('${project.build.directory}/bin/', files), '${project.build.directory}')