import os

import environment
from light_interface import light
from candle_interface import candle
from signtool_interface import sign
from insignia_interface import extract_engine, reattach_engine

engine_tmp_folder = 'obj'

# TODO: #GDM Remove filtered variables

sign_binaries_option = '${windows.skip.sign}' != 'true'
build_nxtool_option = '${nxtool}' == 'true'
build_paxton_option = ('${arch}' == 'x86' and '${paxton}' == 'true')

installer_target_dir = '${installer.target.dir}'
if environment.distribution_output_dir:
    installer_target_dir = environment.distribution_output_dir

client_msi_name = environment.client_distribution_name + '.msi'
server_msi_name = environment.server_distribution_name + '.msi'

wix_extensions = ['WixFirewallExtension', 'WixUtilExtension', 'WixUIExtension',
    'WixBalExtensionExt']

client_components = ['Associations', 'ClientDlg', 'ClientFonts', 'ClientVox', 'ClientBg',
    'ClientQml', 'Client', 'ClientHelp', 'ClientVcrt14', 'MyExitDialog', 'UpgradeDlg',
    'SelectionWarning', 'Product-client-only']
server_components = ['ServerVox', 'Server', 'traytool', 'ServerVcrt14', 'TraytoolVcrt14',
    'MyExitDialog', 'UpgradeDlg', 'SelectionWarning', 'Product-server-only']
nxtool_components = ['NxtoolDlg', 'Nxtool', 'NxtoolQuickControls', 'NxtoolVcrt14',
    'MyExitDialog', 'UpgradeDlg', 'SelectionWarning', 'Product-nxtool']
paxton_components = ['AxClient', 'ClientQml', 'ClientFonts', 'ClientVox', 'PaxtonVcrt14',
    'Product-paxton']

client_exe_components = ['ArchCheck', 'ClientPackage', 'Product-client-exe']
server_exe_components = ['ArchCheck', 'ServerPackage', 'Product-server-exe']
full_exe_components   = ['ArchCheck', 'ClientPackage', 'ServerPackage', 'Product-full-exe']
nxtool_exe_components = ['ArchCheck', 'NxtoolPackage', 'Product-nxtool-exe']
paxton_exe_components = ['PaxtonPackage', 'Product-paxton-exe']

def candle_and_light(project, filename, components, candle_variables):
    candle_output_folder = os.path.join(engine_tmp_folder, project)
    candle(candle_output_folder, components, candle_variables, wix_extensions)
    target_file = os.path.join(installer_target_dir, filename)
    light(target_file, candle_output_folder, wix_extensions)
    return target_file


def build_msi(project, filename, components, candle_variables):
    target_file = candle_and_light(project, filename, components, candle_variables)
    if sign_binaries_option:
        sign(target_file)


def build_exe(project, filename, components, candle_variables):
    target_file = candle_and_light(project, filename, components, candle_variables)
    if sign_binaries_option:
        engine_filename = project + '.engine.exe'
        engine_path = os.path.join(engine_tmp_folder, engine_filename)
        extract_engine(target_file, engine_path),               # Extract engine
        sign(engine_path),                                      # Sign it
        reattach_engine(engine_path, target_file, target_file), # Reattach signed engine
        sign(target_file)                                       # Sign the bundle


def build_client_strip():
    candle_msi_variables = {
        'NoStrip': 'no',
        'ClientFontsSourceDir' : environment.client_fonts_source_dir,
        'VoxSourceDir': environment.vox_source_dir,
        'Vcrt14SrcDir': environment.vcrt14_source_dir,
        'ClientQmlDir': environment.client_qml_source_dir,
        'ClientHelpSourceDir': environment.client_help_source_dir,
        'ClientBgSourceDir': environment.client_background_source_dir
        }
    build_msi('client-msi', client_msi_name, client_components, candle_msi_variables)

    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'ClientMsiName': client_msi_name
        }
    build_exe('client-exe', environment.client_distribution_name + '.exe',
        client_exe_components, candle_exe_variables)


def build_server_strip():
    candle_msi_variables = {
        'NoStrip': 'no',
        'VoxSourceDir': environment.vox_source_dir,
        'Vcrt14SrcDir': environment.vcrt14_source_dir
        }
    build_msi('server-msi', server_msi_name, server_components, candle_msi_variables)

    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'ServerMsiName': server_msi_name
    }
    build_exe('server-exe', environment.server_distribution_name + '.exe',
        server_exe_components, candle_exe_variables)


def build_bundle():
    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'ClientMsiName': client_msi_name,
        'ServerMsiName': server_msi_name
    }
    build_exe('bundle-exe', environment.bundle_distribution_name + '.exe', full_exe_components,
        candle_exe_variables)


def build_nxtool():
    nxtool_msi_name = environment.servertool_distribution_name + '.msi'
    candle_msi_variables = {
        'NxtoolVcrt14DstDir': '${customization}NxtoolDir',
        'NxtoolQuickControlsDir': '${NxtoolQuickControlsDir}',
        'NxtoolQmlDir': '${project.build.directory}/nxtoolqml'
        }
    build_msi('nxtool-msi', nxtool_msi_name, nxtool_components, candle_msi_variables)

    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'NxtoolMsiName': nxtool_msi_name
        }
    build_exe('nxtool-exe', environment.servertool_distribution_name + '.exe',
        nxtool_exe_components, candle_exe_variables)


def build_paxton():
    paxton_msi_name = environment.paxton_distribution_name + '.msi'
    candle_msi_variables = {
        'NoStrip': 'no',
        'VoxSourceDir': environment.vox_source_dir,
        'Vcrt14SrcDir': environment.vcrt14_source_dir,
        'ClientQmlDir': environment.client_qml_source_dir,
        'ClientHelpSourceDir': environment.client_help_source_dir,
        'ClientFontsDir' : environment.client_fonts_source_dir,
        'ClientBgSourceDir': environment.client_background_source_dir,
        'PaxtonVcrt14DstDir': '${customization}_${release.version}.${buildNumber}_Paxton' # WTF
        }
    build_msi('paxton-msi', paxton_msi_name, paxton_components, candle_msi_variables)

    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'PaxtonMsiName': client_msi_name
        }
    build_exe('paxton-exe', environment.paxton_distribution_name + '.exe',
        paxton_exe_components, candle_exe_variables)


def main():
    build_client_strip()
    build_server_strip()
    build_bundle()
    if build_paxton_option:
        build_paxton()

    if build_nxtool_option:
        build_nxtool()

if __name__ == '__main__':
    main()
