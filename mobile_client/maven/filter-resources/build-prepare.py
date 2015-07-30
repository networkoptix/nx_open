import os, shutil

os.system('security unlock-keychain -p 123 ${HOME}/Library/Keychains/login.keychain')

# Copy qml resources to .pro file dir to let qmlimportscanner work properly.
qmlDestDir = os.path.join(os.getcwd(), 'qml')
shutil.rmtree(qmlDestDir, ignore_errors=True)
shutil.copytree('${basedir}/static-resources/qml', qmlDestDir)
