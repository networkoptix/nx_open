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
nxtool_exe_folder = 'bin/exe'

nxtool_msi_folder = 'bin/msi'
server_msi_strip_folder = 'bin/strip'
client_msi_strip_folder = 'bin/strip'
wix_pdb = 'wixsetup.wixpdb'

server_msi_name = '${final.artifact.name}-server.msi'
server_exe_name = '${final.artifact.name}-server.exe'

client_msi_name = '${final.artifact.name}-client.msi'
client_exe_name = '${final.artifact.name}-client.exe'

full_exe_name = '${final.artifact.name}-bundle.exe'

nxtool_msi_name = '${final.artifact.name}-servertool.msi'
nxtool_exe_name = '${final.artifact.name}-servertool.exe'

wix_extensions = ['WixFirewallExtension', 'WixUtilExtension', 'WixUIExtension', 'WixBalExtensionExt']
common_components = ['MyExitDialog', 'UpgradeDlg', 'SelectionWarning']
client_components = ['Associations', 'ClientDlg', 'ClientFonts', 'ClientVox', 'ClientBg', 'ClientQml', 'Client', 'ClientHelp', 'ClientVcrt14']
server_components = ['ServerVox', 'Server', 'traytool', 'ServerVcrt14', 'TraytoolVcrt14']
nxtool_components = ['NxtoolDlg', 'Nxtool', 'NxtoolQuickControls', 'NxtoolVcrt14']

client_exe_components = ['ArchCheck', 'ClientPackage']
server_exe_components = ['ArchCheck', 'ServerPackage']
full_exe_components =   ['ArchCheck', 'ClientPackage', 'ServerPackage']
nxtool_exe_components = ['ArchCheck', 'NxtoolPackage']

def add_wix_extensions(command):
    for ext in wix_extensions:
        command.append('-ext')
        command.append('{0}.dll'.format(ext))

def add_components(command, components):
    for component in components:
        command.append('{0}.wxs'.format(component))

def get_candle_command(project, suffix, args, components):
    command = ['candle']
    command.append('-dClientVoxSourceDir=${ClientVoxSourceDir}')
    command.append('-arch')
    command.append('${arch}')
    command.append('-out')
    command.append('obj\\${build.configuration}-{0}\\'.format(suffix))

    if args:
        command += args

    command.append('-dClientMsiName={}'.format(client_msi_name))
    command.append('-dServerMsiName={}'.format(server_msi_name))
    command.append('-dNxtoolMsiName={}'.format(nxtool_msi_name))

    add_components(command, components)

    command.append(r'-dVcrt14SrcDir=${VC14RedistPath}\bin')
    command.append(r'-dServerVcrt14DstDir=${customization}MediaServerDir')
    command.append(r'-dClientVcrt14DstDir=${customization}_${release.version}.${buildNumber}_Dir')
    command.append(r'-dTraytoolVcrt14DstDir=${customization}TrayToolDir')
    command.append(r'-dNxtoolVcrt14DstDir=${customization}NxtoolDir')

    if suffix.startswith('client'):
        command.append('-dClientQmlDir=${ClientQmlDir}')
        command.append('-dClientHelpSourceDir=${ClientHelpSourceDir}')
        command.append('-dClientFontsDir=${ClientFontsDir}')
        command.append('-dClientBgSourceDir=${ClientBgSourceDir}')

    if suffix.startswith('nxtool'):
        command.append('-dNxtoolQuickControlsDir=${NxtoolQuickControlsDir}')
        command.append('-dNxtoolQmlDir=${project.build.directory}\\nxtoolqml')

    add_wix_extensions(command)
    add_components(command, common_components)
    command.append('Product-{0}.wxs'.format(project))

    return command

def get_light_command(folder, msi, suffix):
    command = ['light']
    command.append('-sice:ICE07')
    command.append('-sice:ICE60')
    command.append('-sice:ICE69')
    command.append('-sice:ICE91')
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

def create_commands_set(project, folder, msi, suffix=None, candle_args=None, components=None):
    if suffix is None:
        suffix = project

    return [
        get_candle_command(project, suffix, candle_args, components),
        get_light_command(folder, msi, suffix)
    ]

def rename(folder, old_name, new_name):
    if os.path.exists(join(folder, new_name)):
        os.unlink(join(folder, new_name))
    if os.path.exists(join(folder, old_name)):
        shutil.copy2(join(folder, old_name), join(folder, new_name))

def add_build_commands_msi_generic(commands, name, msi_folder, msi_name, candle_args, components, suffix=None):
    commands += create_commands_set(name, msi_folder, msi_name, suffix=suffix, candle_args=candle_args, components=components)
    if not skip_sign:
        commands += create_sign_command_set(msi_folder, msi_name)

def add_build_commands_exe_generic(commands, name, exe_folder, exe_name, exe_components, engine_tmp_folder):
    commands += create_commands_set(name, exe_folder, exe_name, components=exe_components)
    if not skip_sign:
        commands += create_sign_burn_exe_command_set(exe_folder, engine_tmp_folder, exe_name)

def add_build_commands_msi_exe_generic(commands, name, name_exe, msi_folder, msi_name, exe_folder, exe_name, candle_args, components, exe_components, engine_tmp_folder, suffix=None):
    add_build_commands_msi_generic(commands, name, msi_folder, msi_name, candle_args, components, suffix)
    add_build_commands_exe_generic(commands, name_exe, exe_folder, exe_name, exe_components, engine_tmp_folder)


def add_build_strip_client_commands(commands):
    add_build_commands_msi_exe_generic(commands,
                'client-only', 'client-exe',
                client_msi_strip_folder, client_msi_name,
                client_exe_folder, client_exe_name,
                ['-dNoStrip=no'],
                client_components, client_exe_components,
                engine_tmp_folder,
                suffix='client-strip')

def add_build_strip_server_commands(commands):
    add_build_commands_msi_exe_generic(commands, 'server-only', 'server-exe',
                server_msi_strip_folder, server_msi_name,
                server_exe_folder, server_exe_name,
                ['-dNoStrip=no'],
                server_components, server_exe_components,
                engine_tmp_folder,
                suffix='server-strip')

def add_build_full_commands(commands):
    add_build_commands_exe_generic(commands, 'full-exe', full_exe_folder, full_exe_name, full_exe_components, engine_tmp_folder)

def add_build_nxtool_commands(commands):
    add_build_commands_msi_exe_generic(commands, 'nxtool', 'nxtool-exe',
                nxtool_msi_folder, nxtool_msi_name,
                nxtool_exe_folder, nxtool_exe_name,
                None,
                nxtool_components, nxtool_exe_components,
                engine_tmp_folder)

def main():
    commands = []

    add_build_strip_client_commands(commands)
    add_build_strip_server_commands(commands)
    add_build_full_commands(commands)

    if build_nxtool:
        add_build_nxtool_commands(commands)

    for command in commands:
        execute_command(command)

    # Debug code to make applauncher work from the build_environment/target/bin folder
    rename(bin_source_dir, 'minilauncher.exe', '${minilauncher.binary.name}')
    rename(bin_source_dir, 'desktop_client.exe', '${client.binary.name}')

if __name__ == '__main__':
    main()
