import os
import sys
import subprocess
import shutil

from os.path import dirname, join, exists, isfile, abspath

from environment import bin_source_dir, rename, execute_command
from light_interface import light_command
from candle_interface import candle_executable
from signtool_interface import sign_command
from insignia_interface import extract_engine_command, reattach_engine_command

engine_tmp_folder = 'obj'

skip_sign = '${windows.skip.sign}' == 'true'
build_nxtool = '${nxtool}' == 'true'
build_paxton = ('${arch}' == 'x86' and '${paxton}' == 'true')

installer_target_dir = '${installer.target.dir}'

server_msi_folder = '${installer.target.dir}/msi'
server_msi_strip_folder = '${installer.target.dir}/strip'
server_exe_folder = '${installer.target.dir}/exe'

client_msi_folder = '${installer.target.dir}/msi'
client_msi_strip_folder = '${installer.target.dir}/strip'
client_exe_folder = '${installer.target.dir}/exe'

full_exe_folder = '${installer.target.dir}/exe'
nxtool_exe_folder = '${installer.target.dir}/exe'

paxton_msi_folder = '${installer.target.dir}/msi'
paxton_msi_strip_folder = '${installer.target.dir}/strip'
paxton_exe_folder = '${installer.target.dir}/exe'

nxtool_msi_folder = '${installer.target.dir}/msi'

server_msi_name = '${artifact.name.server}.msi'
server_exe_name = '${artifact.name.server}.exe'

client_msi_name = '${artifact.name.client}.msi'
client_exe_name = '${artifact.name.client}.exe'

full_exe_name = '${artifact.name.bundle}.exe'

nxtool_msi_name = '${artifact.name.servertool}.msi'
nxtool_exe_name = '${artifact.name.servertool}.exe'

paxton_msi_name = '${artifact.name.paxton}.msi'
paxton_exe_name = '${artifact.name.paxton}.exe'

wix_extensions = ['WixFirewallExtension', 'WixUtilExtension', 'WixUIExtension', 'WixBalExtensionExt']
client_components = ['Associations', 'ClientDlg', 'ClientFonts', 'ClientVox', 'ClientBg', 'ClientQml', 'Client', 'ClientHelp', 'ClientVcrt14', 'MyExitDialog', 'UpgradeDlg', 'SelectionWarning']
server_components = ['ServerVox', 'Server', 'traytool', 'ServerVcrt14', 'TraytoolVcrt14', 'MyExitDialog', 'UpgradeDlg', 'SelectionWarning']
nxtool_components = ['NxtoolDlg', 'Nxtool', 'NxtoolQuickControls', 'NxtoolVcrt14', 'MyExitDialog', 'UpgradeDlg', 'SelectionWarning']
paxton_components = ['AxClient', 'ClientQml', 'ClientFonts', 'ClientVox', 'PaxtonVcrt14']

client_exe_components = ['ArchCheck', 'ClientPackage']
server_exe_components = ['ArchCheck', 'ServerPackage']
full_exe_components =   ['ArchCheck', 'ClientPackage', 'ServerPackage']
nxtool_exe_components = ['ArchCheck', 'NxtoolPackage']
paxton_exe_components = ['PaxtonPackage']

def add_wix_extensions(command):
    for ext in wix_extensions:
        command.append('-ext')
        command.append('{0}.dll'.format(ext))

def add_components(command, components):
    for component in components:
        command.append('{0}.wxs'.format(component))

def get_candle_command(project, suffix, args, components):
    command = [candle_executable()]
    command.append('-dClientVoxSourceDir=${ClientVoxSourceDir}')
    command.append(r'-dVcrt14SrcDir=${VC14RedistPath}\bin')
    
    command.append('-arch')
    command.append('${arch}')
    command.append('-out')
    command.append('obj\\${build.configuration}-{0}\\'.format(suffix))

    if args:
        command += args

    command.append('-dClientMsiName={}'.format(client_msi_name))
    command.append('-dServerMsiName={}'.format(server_msi_name))
    command.append('-dNxtoolMsiName={}'.format(nxtool_msi_name))
    command.append('-dPaxtonMsiName={}'.format(paxton_msi_name))
    command.append('-dInstallerTargetDir={}'.format(installer_target_dir))

    add_components(command, components)

    command.append(r'-dNxtoolVcrt14DstDir=${customization}NxtoolDir')
    command.append(r'-dPaxtonVcrt14DstDir=${customization}_${release.version}.${buildNumber}_Paxton')

    if suffix.startswith('client'):
        command.append('-dClientQmlDir=${ClientQmlDir}')
        command.append('-dClientHelpSourceDir=${ClientHelpSourceDir}')
        command.append('-dClientFontsDir=${ClientFontsDir}')
        command.append('-dClientBgSourceDir=${ClientBgSourceDir}')

    if suffix.startswith('paxton'):
        command.append('-dClientQmlDir=${ClientQmlDir}')
        command.append('-dClientFontsDir=${ClientFontsDir}')

    if suffix.startswith('nxtool'):
        command.append('-dNxtoolQuickControlsDir=${NxtoolQuickControlsDir}')
        command.append('-dNxtoolQmlDir=${project.build.directory}\\nxtoolqml')

    add_wix_extensions(command)
    command.append('Product-{0}.wxs'.format(project))

    return command

def get_sign_command(folder, msi):
    target_file = '{0}/{1}'.format(folder, msi)
    main_certificate = '${certificates_path}/wixsetup/${sign.cer}'
    additional_certificate = '${sign.intermediate.cer}'
    password = '${sign.password}'
    description = '"${company.name} ${display.product.name}"'   
    return sign_command(target_file, main_certificate, additional_certificate, password, description)

def create_sign_command_set(folder, msi):
    return [get_sign_command(folder, msi)]

def get_extract_engine_command(bundle, output):
    return extract_engine_command(bundle, output)

def get_bundle_engine_command(engine, bundle):
    return reattach_engine_command(engine, bundle, bundle)

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

def create_commands_set(project, folder, msi, suffix=None, candle_args=None, components=None):
    if suffix is None:
        suffix = project

    return [
        get_candle_command(project, suffix, candle_args, components),
        light_command(folder, msi, suffix, wix_extensions)
    ]

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

def add_build_paxton_commands(commands):
    add_build_commands_msi_exe_generic(commands, 'paxton', 'paxton-exe',
                paxton_msi_strip_folder, paxton_msi_name,
                paxton_exe_folder, paxton_exe_name,
                ['-dNoStrip=no'],
                paxton_components, paxton_exe_components,
                engine_tmp_folder,
                suffix='paxton-strip')

def main():
    commands = []

    add_build_strip_client_commands(commands)
    add_build_strip_server_commands(commands)
    add_build_full_commands(commands)
    if build_paxton:
        add_build_paxton_commands(commands)

    if build_nxtool:
        add_build_nxtool_commands(commands)

    for command in commands:
        execute_command(command, True)

    # Debug code to make applauncher work from the build_environment/target/bin folder
    rename(bin_source_dir, 'minilauncher.exe', '${minilauncher.binary.name}')
    rename(bin_source_dir, 'desktop_client.exe', '${client.binary.name}')

if __name__ == '__main__':
    main()
