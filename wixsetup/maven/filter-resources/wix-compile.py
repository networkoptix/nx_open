import os, sys, subprocess, shutil
from subprocess import Popen, PIPE

commands = [\
'candle -dinstalltype="%s" -dAppServerSourceDir="${AppServerDir}" -dClientHelpSourceDir="${ClientHelpSourceDir}" -dClientVoxSourceDir="${ClientVoxSourceDir}" -dClientBgSourceDir="${ClientBgSourceDir}" -arch ${arch} -out obj\${build.configuration}\ -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll UninstallOptionsDlg.wxs Associations.wxs MyExitDialog.wxs PasswordShouldMatchDlg.wxs UpgradeDlg.wxs PasswordDlg.wxs EmptyPasswordDlg.wxs AllFieldsAreMandatoryDlg.wxs InstallTypeDlg.wxs PortDuplicateDlg.wxs PortIsBusyDlg.wxs InvalidPasswordDlg.wxs MediaDirDlg.wxs MediaServerDlg.wxs ClientDlg.wxs MyFeaturesDlg.wxs SelectionWarning.wxs DowngradeWarningDlg.wxs Product-all.wxs AppServerFiles.wxs ClientHelp.wxs ClientVox.wxs ClientBg.wxs AppServerDlg.wxs Client.wxs' % config,\
'light -cultures:en-US -loc CustomStrings.wxl -ext WixFirewallExtension.dll -ext WixUIExtension.dll -ext WixUtilExtension.dll -ext wixext\WixSystemToolsExtension.dll -out bin/${project.build.finalName}-full.msi -pdbout bin\${build.configuration}\EVEMediaPlayerSetup.wixpdb obj\${build.configuration}\*.wixobj"',\
'cscript FixExitDialog.js bin/${project.build.finalName}-full.msi'\
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