import os
import sys
import subprocess
import shutil

from os.path import dirname, join, exists, isfile, abspath

engine_tmp_folder = 'obj'

skip_sign = '${windows.skip.sign}' == 'true'
build_nxtool = '${nxtool}' == 'true'

bin_source_dir = '${libdir}/bin/${build.configuration}'

server_msi_folder = 'bin/msi'
server_exe_folder = 'bin/exe'

client_msi_folder = 'bin/msi'
client_exe_folder = 'bin/exe'

full_exe_folder = 'bin/exe'

nxtool_msi_folder = 'bin/msi'
server_msi_strip_folder = 'bin/strip'
client_msi_strip_folder = 'bin/strip'
wix_pdb = 'wixsetup.wixpdb'

server_msi_name = '${finalName}-server-only.msi'
server_exe_name = '${finalName}-server-only.exe'

client_msi_name = '${finalName}-client-only.msi'
client_exe_name = '${finalName}-client-only.exe'

full_exe_name = '${finalName}.exe'

nxtool_msi_name = '${finalName}-servertool.msi'

wix_extensions = ['WixFirewallExtension', 'WixUtilExtension', 'WixUIExtension', 'WixBalExtension', 'wixext\WixSystemToolsExtension']
common_components = ['MyExitDialog', 'UpgradeDlg', 'SelectionWarning']
client_components = ['Associations', 'ClientDlg', 'ClientFonts', 'ClientVox', 'ClientBg', 'ClientQml', 'Client', 'ClientHelp']
server_components = ['UninstallOptionsDlg', 'ServerVox', 'Server', 'traytool']
nxtool_components = ['NxtoolDlg', 'Nxtool', 'NxtoolQuickControls']

def add_wix_extensions(command):
    for ext in wix_extensions:
        command.append('-ext')
        command.append('{0}.dll'.format(ext))

def add_components(command, components):
    for component in components:
        command.append('{0}.wxs'.format(component))

def get_candle_command(suffix):
    command = ['candle']
    command.append('-dClientVoxSourceDir=${ClientVoxSourceDir}')
    command.append('-arch')
    command.append('${arch}')
    command.append('-out')
    command.append('obj\\${build.configuration}-{0}\\'.format(suffix))

    command.append('-dClientMsiName={}'.format(client_msi_name))
    command.append('-dServerMsiName={}'.format(server_msi_name))

    if suffix.startswith('client'):
        command.append('-dClientQmlDir=${ClientQmlDir}')
        command.append('-dClientHelpSourceDir=${ClientHelpSourceDir}')
        command.append('-dClientFontsDir=${ClientFontsDir}')
        command.append('-dClientBgSourceDir=${ClientBgSourceDir}')
        add_components(command, client_components)

    if suffix.startswith('server'):
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
    command.append('${libdir}/bin/${build.configuration}/cab')
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

def get_sign_command(folder, msi):
    command = ['sign.bat']
    command.append('{0}/{1}'.format(folder, msi))
    return command

def create_sign_command_set(folder, msi):
    return [get_sign_command(folder, msi)]

def get_extract_engine_command(exe, out):
    command = ['insignia']
    command.append('-ib')
    command.append(exe)
    command.append('-o')
    command.append(out)
    return command

def get_bundle_engine_command(engine, out):
    command = ['insignia']
    command.append('-ab')
    command.append(engine)
    command.append(out)
    command.append('-o')
    command.append(out)
    return command

def get_remove_file_command(file_path):
    command = ['del']
    command.append(abspath(file_path))
    return command

def create_sign_burn_exe_command_set(folder, engine_folder, exe):
    engine_filename = exe + '.engine.exe'

    exe_path = join(folder, exe)
    engine_path = join(engine_folder, engine_filename)

    return [
        get_extract_engine_command(exe_path, engine_path),
        get_sign_command(engine_folder, engine_filename),
        get_bundle_engine_command(engine_path, exe_path),
        get_sign_command(folder, exe)
    ]
    
def execute_command(command):
    print 'Executing command:\n{0}\n'.format(' '.join(command))
    retcode = subprocess.call(command)
    print "finished with return code {0}".format(retcode)
    if retcode != 0 and retcode != 204:
        sys.exit(1)
    
def create_commands_set(project, folder, msi):
    return [
        get_candle_command(project),
        get_light_command(folder, msi, project)
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

    if not skip_sign:
        commands += create_sign_command_set(client_msi_folder, client_msi_name)
        commands += create_sign_command_set(server_msi_folder, server_msi_name)
        commands += create_sign_command_set(client_msi_strip_folder, client_msi_name)
        commands += create_sign_command_set(server_msi_strip_folder, server_msi_name)

    commands += create_commands_set('client-exe', client_exe_folder, client_exe_name)
    commands += create_commands_set('server-exe', server_exe_folder, server_exe_name)
    commands += create_commands_set('full-exe', full_exe_folder, full_exe_name)

    if not skip_sign:
        commands += create_sign_burn_exe_command_set(client_exe_folder, engine_tmp_folder, client_exe_name)
        commands += create_sign_burn_exe_command_set(server_exe_folder, engine_tmp_folder, server_exe_name)
        commands += create_sign_burn_exe_command_set(full_exe_folder, engine_tmp_folder, full_exe_name)

    if build_nxtool:
        commands += create_commands_set('nxtool', nxtool_msi_folder, nxtool_msi_name)
        if not skip_sign:
            commands += create_sign_command_set(nxtool_msi_folder, nxtool_msi_name)

    for command in commands:
        execute_command(command)

    # Debug code to make applauncher work from the build_environment/target/bin folder
    rename(bin_source_dir, 'minilauncher.exe', '${minilauncher.binary.name}')
    rename(bin_source_dir, 'desktop_client.exe', '${client.binary.name}')

if __name__ == '__main__':
    main()
