import os
import sys

import argparse
import yaml

import environment
from light_interface import light
from candle_interface import candle
from insignia_interface import extract_engine, reattach_engine

engine_tmp_folder = 'obj'

# TODO: #GDM Remove filtered variables

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

wix_extensions = [
    'WixFirewallExtension',
    'WixUtilExtension',
    'WixUIExtension',
    'WixBalExtension'
]

shared_vms_components = [
    'shared_nx_libraries',
    'shared_misc_libraries',
    'shared_qt_libraries',
    'shared_qt_platform_libraries',
    'shared_icu_libraries',
    'shared_ucrt_libraries',
    'shared_vcrt_libraries',
    'vox',
    'MyExitDialog',
    'UpgradeDlg',
    'SelectionWarning'
]

client_components = shared_vms_components + [
    'client_libraries',
    'Associations',
    'ClientDlg',
    'client/msi_interface',
    'ClientFonts',
    'ClientBg',
    'ClientQml',
    'Client',
    'ClientHelp',
    'Product-client-only']

server_components = shared_vms_components + [
    'server_libraries',
    'Server',
    'traytool',
    'Product-server-only']

client_exe_components = ['ArchCheck', 'ClientPackage', 'Product-client-exe']
server_exe_components = ['ArchCheck', 'ServerPackage', 'Product-server-exe']
full_exe_components = ['ArchCheck', 'ClientPackage', 'ServerPackage', 'Product-full-exe']


def sign(output_file, code_signing):
    url = code_signing['url']
    customization = code_signing['customization']
    trusted_timestamping = code_signing['trusted_timestamping']
    script = code_signing['script']

    script_path, script_name = os.path.split(script)
    sys.path.insert(0, script_path)
    from sign_binary import sign_binary
    sys.path.pop(0)
    sign_binary(url, output_file, output_file, customization, trusted_timestamping)


def candle_and_light(project, filename, components, candle_variables, external_components=None):
    candle_output_folder = os.path.join(engine_tmp_folder, project)
    candle(candle_output_folder, components, candle_variables, wix_extensions)

    light_sources = [
        '{}/*.wixobj'.format(candle_output_folder),
    ]
    if external_components:
        light_sources += external_components
    light(filename, light_sources, wix_extensions)


def build_msi(project, filename, components, candle_variables, config, external_components=None):
    candle_and_light(project, filename, components, candle_variables, external_components)
    code_signing = config['code_signing']
    if code_signing['enabled']:
        sign(filename, code_signing)


def build_exe(project, filename, components, candle_variables, config, external_components=None):
    candle_and_light(project, filename, components, candle_variables, external_components)
    code_signing = config['code_signing']
    if code_signing['enabled']:
        engine_filename = project + '.engine.exe'
        engine_path = os.path.join(engine_tmp_folder, engine_filename)
        extract_engine(filename, engine_path),             # Extract engine
        sign(engine_path, code_signing),                   # Sign it
        reattach_engine(engine_path, filename, filename),  # Reattach signed engine
        sign(filename, code_signing)                       # Sign the bundle


def set_embedded_cabs(params, value):
    params['NoStrip'] = ('yes' if value else 'no')


def build_client(config):

    client_common_components = [
        'webengine_resources.wixobj'
    ]

    client_common_components_paths = [
        os.path.join('desktop_client', component)
        for component in client_common_components
    ]

    candle_msi_variables = {
        'ClientFontsSourceDir': environment.client_fonts_source_dir,
        'VoxSourceDir': environment.vox_source_dir,
        'ClientQmlDir': environment.client_qml_source_dir,
        'ClientHelpSourceDir': environment.client_help_source_dir,
        'ClientBgSourceDir': environment.client_background_source_dir
    }
    set_embedded_cabs(candle_msi_variables, True)
    build_msi(
        'client-msi',
        client_msi_file,
        client_components,
        candle_msi_variables,
        config,
        external_components=client_common_components_paths)

    set_embedded_cabs(candle_msi_variables, False)
    build_msi(
        'client-strip',
        client_stripped_file,
        client_components,
        candle_msi_variables,
        config,
        external_components=client_common_components_paths)

    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'ClientMsiName': client_msi_name,
        'ClientStrippedMsiFile': client_stripped_file
    }
    build_exe(
        'client-exe',
        client_exe_file,
        client_exe_components,
        candle_exe_variables,
        config)


def build_server(config):
    candle_msi_variables = {
        'VoxSourceDir': environment.vox_source_dir
    }
    set_embedded_cabs(candle_msi_variables, True)
    build_msi('server-msi', server_msi_file, server_components, candle_msi_variables, config)

    set_embedded_cabs(candle_msi_variables, False)
    build_msi('server-msi', server_stripped_file, server_components, candle_msi_variables, config)

    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'ServerMsiName': server_msi_name,
        'ServerStrippedMsiFile': server_stripped_file
    }
    build_exe(
        'server-exe',
        server_exe_file,
        server_exe_components,
        candle_exe_variables,
        config)


def build_bundle(config):
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
        candle_exe_variables,
        config)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f, Loader=yaml.SafeLoader)

    build_client(config)
    build_server(config)
    build_bundle(config)


if __name__ == '__main__':
    main()
