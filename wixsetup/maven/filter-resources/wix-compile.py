import os

import environment
from light_interface import light
from candle_interface import candle
from signtool_interface import sign
from insignia_interface import extract_engine, reattach_engine

engine_tmp_folder = 'obj'

# TODO: #GDM Remove filtered variables

sign_binaries_option = '${windows.skip.sign}' != 'true'
build_paxton_option = ('${arch}' == 'x86' and '${paxton}' == 'true')

installer_target_dir = '${installer.target.dir}'
if environment.distribution_output_dir:
    installer_target_dir = environment.distribution_output_dir

client_msi_name = environment.client_distribution_name + '.msi'
client_exe_name = environment.client_distribution_name + '.exe'
client_stripped_file = os.path.join(engine_tmp_folder, 'client-strip.msi')
client_msi_file = os.path.join(installer_target_dir, client_msi_name)
client_exe_file = os.path.join(installer_target_dir, client_exe_name)

server_msi_name = environment.server_distribution_name + '.msi'
server_exe_name = environment.server_distribution_name + '.exe'
server_stripped_file = os.path.join(engine_tmp_folder, 'server-strip.msi')
server_msi_file = os.path.join(installer_target_dir, server_msi_name)
server_exe_file = os.path.join(installer_target_dir, server_exe_name)

bundle_exe_name = environment.bundle_distribution_name + '.exe'
bundle_exe_file = os.path.join(installer_target_dir, bundle_exe_name)

paxton_stripped_file = os.path.join(engine_tmp_folder, 'paxton-strip.msi')
paxton_exe_name = environment.paxton_distribution_name + '.exe'
paxton_exe_file = os.path.join(installer_target_dir, paxton_exe_name)

wix_extensions = [
    'WixFirewallExtension',
    'WixUtilExtension',
    'WixUIExtension',
    'WixBalExtensionExt']

client_components = [
    'Associations',
    'ClientDlg',
    'ClientFonts',
    'ClientVox',
    'ClientBg',
    'ClientQml',
    'Client',
    'ClientHelp',
    'ClientVcrt14',
    'MyExitDialog',
    'UpgradeDlg',
    'SelectionWarning',
    'Product-client-only']

server_components = [
    'ServerVox',
    'Server',
    'traytool',
    'ServerVcrt14',
    'TraytoolVcrt14',
    'MyExitDialog',
    'UpgradeDlg',
    'SelectionWarning',
    'Product-server-only']

paxton_components = [
    'AxClient',
    'ClientQml',
    'ClientFonts',
    'ClientVox',
    'PaxtonVcrt14',
    'Product-paxton']

client_exe_components = ['ArchCheck', 'ClientPackage', 'Product-client-exe']
server_exe_components = ['ArchCheck', 'ServerPackage', 'Product-server-exe']
full_exe_components = ['ArchCheck', 'ClientPackage', 'ServerPackage', 'Product-full-exe']
paxton_exe_components = ['PaxtonPackage', 'Product-paxton-exe']


def candle_and_light(project, filename, components, candle_variables):
    candle_output_folder = os.path.join(engine_tmp_folder, project)
    candle(candle_output_folder, components, candle_variables, wix_extensions)
    light(filename, candle_output_folder, wix_extensions)


def build_msi(project, filename, components, candle_variables):
    candle_and_light(project, filename, components, candle_variables)
    if sign_binaries_option:
        sign(filename)


def build_exe(project, filename, components, candle_variables):
    candle_and_light(project, filename, components, candle_variables)
    if sign_binaries_option:
        engine_filename = project + '.engine.exe'
        engine_path = os.path.join(engine_tmp_folder, engine_filename)
        extract_engine(filename, engine_path),             # Extract engine
        sign(engine_path),                                 # Sign it
        reattach_engine(engine_path, filename, filename),  # Reattach signed engine
        sign(filename)                                     # Sign the bundle


def set_embedded_cabs(params, value):
    params['NoStrip'] = ('yes' if value else 'no')


def build_client():
    candle_msi_variables = {
        'ClientFontsSourceDir': environment.client_fonts_source_dir,
        'VoxSourceDir': environment.vox_source_dir,
        'Vcrt14SrcDir': environment.vcrt14_source_dir,
        'ClientQmlDir': environment.client_qml_source_dir,
        'ClientHelpSourceDir': environment.client_help_source_dir,
        'ClientBgSourceDir': environment.client_background_source_dir
    }
    set_embedded_cabs(candle_msi_variables, True)
    build_msi('client-msi', client_msi_file, client_components, candle_msi_variables)

    set_embedded_cabs(candle_msi_variables, False)
    build_msi('client-strip', client_stripped_file, client_components, candle_msi_variables)

    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'ClientMsiName': client_msi_name,
        'ClientStrippedMsiFile': client_stripped_file
    }
    build_exe(
        'client-exe',
        client_exe_file,
        client_exe_components,
        candle_exe_variables)


def build_server():
    candle_msi_variables = {
        'VoxSourceDir': environment.vox_source_dir,
        'Vcrt14SrcDir': environment.vcrt14_source_dir
    }
    set_embedded_cabs(candle_msi_variables, True)
    build_msi('server-msi', server_msi_file, server_components, candle_msi_variables)

    set_embedded_cabs(candle_msi_variables, False)
    build_msi('server-msi', server_stripped_file, server_components, candle_msi_variables)

    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'ServerMsiName': server_msi_name,
        'ServerStrippedMsiFile': server_stripped_file
    }
    build_exe(
        'server-exe',
        server_exe_file,
        server_exe_components,
        candle_exe_variables)


def build_bundle():
    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'ClientMsiName': client_msi_name,
        'ServerMsiName': server_msi_name,
        'ClientStrippedMsiFile': client_stripped_file,
        'ServerStrippedMsiFile': server_stripped_file
    }
    build_exe(
        'bundle-exe',
        bundle_exe_file,
        full_exe_components,
        candle_exe_variables)


def build_paxton():
    candle_msi_variables = {
        'VoxSourceDir': environment.vox_source_dir,
        'Vcrt14SrcDir': environment.vcrt14_source_dir,
        'ClientQmlDir': environment.client_qml_source_dir,
        'ClientHelpSourceDir': environment.client_help_source_dir,
        'ClientFontsDir': environment.client_fonts_source_dir,
        'ClientBgSourceDir': environment.client_background_source_dir,
        'PaxtonVcrt14DstDir': '${customization}_${release.version}.${buildNumber}_Paxton'  # WTF
    }
    set_embedded_cabs(candle_msi_variables, False)
    build_msi('paxton-msi', paxton_stripped_file, paxton_components, candle_msi_variables)

    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'PaxtonMsiName': client_msi_name,
        'PaxtonStrippedMsiFile': paxton_stripped_file
    }
    build_exe(
        'paxton-exe',
        paxton_exe_file,
        paxton_exe_components,
        candle_exe_variables)


def main():
    build_client()
    build_server()
    build_bundle()
    if build_paxton_option:
        build_paxton()


if __name__ == '__main__':
    main()
