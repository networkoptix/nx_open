import os, shutil

if "${platform}" == "ios":
    os.system('security unlock-keychain -p 123 ${HOME}/Library/Keychains/login.keychain')
    profiles_dir = os.path.join(os.getenv("HOME"), "Library/MobileDevice/Provisioning Profiles")
    dst_file = os.path.join(profiles_dir, "${provisioning_profile_id}" + ".mobileprovision")
    shutil.copyfile("${provisioning_profile}", dst_file)

# Copy qml resources to .pro file dir to let qmlimportscanner work properly.
qmlDestDir = os.path.join(os.getcwd(), 'qml')
shutil.rmtree(qmlDestDir, ignore_errors=True)
shutil.copytree('${basedir}/static-resources/qml', qmlDestDir)
