#!/bin/python2

import argparse
import os
import yaml

from heat_interface import harvest_dir
from light_interface import light
from candle_interface import candle
from signtool_interface import sign
#from insignia_interface import extract_engine, reattach_engine

engine_tmp_folder = 'obj'

wix_extensions = [
    'WixFirewallExtension',
    'WixUtilExtension',
    'WixUIExtension',
    'WixBalExtension']

nxtool_components = [
    'NxTool/NxToolDlg',
    'NxTool/NxTool',
    'NxTool/NxToolQml',
    'NxTool/NxToolVcrt14',
    'NxTool/Product',
    'MyExitDialog',
    'UpgradeDlg',
    'SelectionWarning']

nxtool_exe_components = [
    'ArchCheck',
    'NxTool/Package',
    'NxTool/Bundle']

installer_language_pattern = 'OptixTheme_{}.wxl'


def candle_and_light(
    wix_directory,
    sources_directory,
    project,
    arch,
    output_file,
    components,
    candle_variables,
    installer_cultures,
    installer_language
):
    tmp_folder = os.path.join(sources_directory, engine_tmp_folder)
    candle_output_folder = os.path.join(tmp_folder, project)
    candle(
        wix_directory=wix_directory,
        arch=arch,
        components=[os.path.join(sources_directory, c) for c in components],
        variables=candle_variables,
        extensions=wix_extensions,
        output_folder=candle_output_folder)

    light(
        working_directory=sources_directory,
        wix_directory=wix_directory,
        input_folder=candle_output_folder,
        installer_cultures=installer_cultures,
        installer_language=os.path.join(sources_directory, installer_language),
        extensions=wix_extensions,
        output_file=output_file)


def build_msi(
    wix_directory,
    sources_directory,
    project,
    arch,
    output_file,
    components,
    candle_variables,
    installer_cultures,
    installer_language,
    code_signing
):
    candle_and_light(
        wix_directory=wix_directory,
        sources_directory=sources_directory,
        project=project,
        arch=arch,
        output_file=output_file,
        components=components,
        candle_variables=candle_variables,
        installer_cultures=installer_cultures,
        installer_language=installer_language)

    if code_signing['enabled']:
        sign(
            signtool_directory=code_signing['signtool_directory'],
            target_file=output_file,
            sign_description=code_signing['sign_description'],
            sign_password=code_signing['sign_password'],
            main_certificate=code_signing['main_certificate'],
            additional_certificate=code_signing['additional_certificate'])

'''
def build_exe(project, filename, components, candle_variables):
    candle_and_light(project, filename, components, candle_variables)
    if sign_binaries:
        engine_filename = project + '.engine.exe'
        engine_path = os.path.join(engine_tmp_folder, engine_filename)
        extract_engine(filename, engine_path),             # Extract engine
        sign(engine_path),                                 # Sign it
        reattach_engine(engine_path, filename, filename),  # Reattach signed engine
        sign(filename)                                     # Sign the bundle
'''


def create_nxtool_installer(
    wix_directory,
    qml_directory,
    vcredist_directory,
    sources_directory,
    arch,
    installer_cultures,
    installer_language,
    code_signing,
    nxtool_msi_file
):
    harvest_dir(
        wix_directory=wix_directory,
        source_dir=qml_directory,
        target_file=os.path.join(sources_directory, 'NxTool/NxToolQml.wxs'),
        component_group_name='QmlComponentGroup',
        directory_ref='NxToolDstQmlDir',
        source_dir_var='var.NxToolQmlDir')
    harvest_dir(
        wix_directory=wix_directory,
        source_dir=vcredist_directory,
        target_file=os.path.join(sources_directory, 'NxTool/NxToolVcrt14.wxs'),
        component_group_name='Vcrt14ComponentGroup',
        directory_ref='NxToolDstRootDir',
        source_dir_var='var.Vcrt14SrcDir')

    candle_msi_variables = {
        'Vcrt14SrcDir': vcredist_directory,
        'NxToolQmlDir': qml_directory
    }
    build_msi(
        wix_directory=wix_directory,
        sources_directory=sources_directory,
        project='nxtool-msi',
        arch=arch,
        output_file=nxtool_msi_file,
        components=nxtool_components,
        candle_variables=candle_msi_variables,
        installer_cultures=installer_cultures,
        installer_language=installer_language,
        code_signing=code_signing)
'''
    candle_exe_variables = {
        'InstallerTargetDir': installer_target_dir,
        'NxtoolMsiName': nxtool_msi_name,
        'NxToolMsiFile': nxtool_msi_file
    }
    build_exe(
        'nxtool-exe',
        nxtool_exe_file,
        nxtool_exe_components,
        candle_exe_variables)
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--qml-dir', help="Qml directory", required=True)
    parser.add_argument('--sources-dir', help="Sources directory", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    output_file = os.path.join(
        args.output, config['servertool_distribution_name']) + '.msi'

    installer_cultures = config['installer_cultures']
    installer_language = installer_language_pattern.format(config['installer_language'])

#    nxtool_msi_name = config['servertool_distribution_name'] + '.msi'
#    nxtool_msi_file = os.path.join(engine_tmp_folder, 'nxtool.msi')
#    nxtool_exe_name = config['servertool_distribution_name'] + '.exe'
#    nxtool_exe_file = os.path.join(installer_target_dir, nxtool_exe_name)

    create_nxtool_installer(
        wix_directory=config['wix_directory'],
        qml_directory=args.qml_dir,
        vcredist_directory=config['vcredist_directory'],
        sources_directory=args.sources_dir,
        arch=config['arch'],
        installer_cultures=installer_cultures,
        installer_language=installer_language,
        code_signing=config['code_signing'],
        nxtool_msi_file=output_file)


if __name__ == '__main__':
    main()
