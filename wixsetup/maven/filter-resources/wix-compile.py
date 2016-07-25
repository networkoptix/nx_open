import os, sys, subprocess, shutil
from os.path import dirname, join, exists, isfile

bin_source_dir = '${libdir}/${arch}/bin/${build.configuration}'
server_msi_folder = 'bin/msi'
client_msi_folder = 'bin/msi'
nxtool_msi_folder = 'bin/msi'
server_msi_strip_folder = 'bin/strip'
client_msi_strip_folder = 'bin/strip'
wix_pdb = 'wixsetup.wixpdb'

server_msi_name = '${finalName}-server-only.msi'
client_msi_name = '${finalName}-client-only.msi'
nxtool_msi_name = '${finalName}-servertool.msi'

wix_extensions = ['WixFirewallExtension', 'WixUtilExtension', 'WixUIExtension', 'WixBalExtension', 'wixext\WixSystemToolsExtension']
common_components = ['MyExitDialog', 'UpgradeDlg', 'SelectionWarning', 'vs2015crt']
client_components = ['Associations', 'ClientDlg', 'ClientFonts', 'ClientVox', 'ClientBg', 'ClientQml', 'Client', 'ClientHelp']
server_components = ['UninstallOptionsDlg', 'EmptyPasswordDlg', 'MediaServerDlg', 'ServerVox', 'Server', 'traytool', 'DbSync22Files']
nxtool_components = ['NxtoolDlg', 'Nxtool', 'NxtoolQuickControls']

def add_wix_extensions(command):
    for ext in wix_extensions:
        command.append('-ext')
        command.append('{0}.dll'.format(ext))

def add_components(command, components):
    for component in components:
        command.append('{0}.wxs'.format(component))

def get_candle_command(suffix):
    install_type = 'client-only'
    if suffix.startswith('server'):
        install_type = 'server-only'

    command = ['candle']
    command.append('-dinstalltype="{0}"'.format(install_type))
    command.append('-dVs2015crtDir=${Vs2015crtDir}')   
    command.append('-dClientVoxSourceDir=${ClientVoxSourceDir}')
    command.append('-arch')
    command.append('${arch}')
    command.append('-out')
    command.append('obj\\${build.configuration}-{0}\\'.format(suffix))

    if suffix.startswith('client'):
        command.append('-dClientQmlDir=${ClientQmlDir}')
        command.append('-dClientHelpSourceDir=${ClientHelpSourceDir}')
        command.append('-dClientFontsDir=${ClientFontsDir}')
        command.append('-dClientBgSourceDir=${ClientBgSourceDir}')
        add_components(command, client_components)

    if suffix.startswith('server'):
        command.append('-dDbSync22SourceDir=${DbSync22Dir}')
        add_components(command, server_components)

    if suffix.startswith('nxtool'):
        command.append('-dNxtoolQuickControlsDir=${NxtoolQuickControlsDir}')
        command.append('-dNxtoolQmlDir=${project.build.directory}\\nxtoolqml')
        add_components(command, nxtool_components)
        
    add_wix_extensions(command)
    add_components(command, common_components)
    command.append('Product-{0}.wxs'.format(suffix))

    return command

def get_light_command(folder, msi, suffix):
    command = ['light']
    command.append('-cultures:${installer.language}')
    command.append('-cc')
    command.append('${libdir}/${arch}/bin/${build.configuration}/cab')
    command.append('-reusecab')
    command.append('-loc')
    command.append('CustomStrings_${installer.language}.wxl')
    command.append('-out')
    command.append('{0}/{1}'.format(folder, msi))
    command.append('-pdbout')
    command.append('{0}\\${build.configuration}\\{1}'.format(folder, wix_pdb))
    command.append('obj\\${build.configuration}-{0}\\*.wixobj'.format(suffix))

    add_wix_extensions(command)
    return command

def get_fix_dialog_command(folder, msi):
    command = ['cscript']
    command.append('FixExitDialog.js')
    command.append('{0}/{1}'.format(folder, msi))
    return command

def get_iss_command():
    command = [os.path.join(os.getenv('environment'), "is5/ISCC.exe")]
    command.append('/obin')
    command.append('/f${finalName}')
    command.append('Product-full.iss')
    return command
    
def execute_command(command):
    print 'Executing command:\n{0}\n'.format(' '.join(command))
    retcode = subprocess.call(command)
    print "finished with return code {0}".format(retcode)
    if retcode != 0 and retcode != 204:
        sys.exit(1)
    
def create_commands_set(project, folder, msi):
    return [
        get_candle_command(project),
        get_light_command(folder, msi, project),
        get_fix_dialog_command(folder, msi),
    ]
    
def rename(folder, old_name, new_name):
    if os.path.exists(join(folder, new_name)):
        os.unlink(join(folder, new_name))
    if os.path.exists(join(folder, old_name)):
        shutil.copy2(join(folder, old_name), join(folder, new_name))
    
def main():
    commands = []
    commands += create_commands_set('client-only', client_msi_folder, client_msi_name)
    commands += create_commands_set('server-only', server_msi_folder, server_msi_name)
    commands += create_commands_set('client-strip', client_msi_strip_folder, client_msi_name)
    commands += create_commands_set('server-strip', server_msi_strip_folder, server_msi_name)   
    if '${nxtool}' == 'true':
        commands += create_commands_set('nxtool', nxtool_msi_folder, nxtool_msi_name)

    for command in commands:
        execute_command(command)

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

    execute_command(get_iss_command())

    #Debug code to make applauncher work from the build_environment/target/bin folder
    rename(bin_source_dir, 'minilauncher.exe', '${minilauncher.binary.name}')
    rename(bin_source_dir, 'desktop_client.exe', '${client.binary.name}')

if __name__ == '__main__':
    main()
