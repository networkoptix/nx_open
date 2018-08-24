#!/bin/python2

import argparse
import os
import yaml

from heat_interface import harvest_dir
from light_interface import light
from candle_interface import candle
from signtool_interface import sign_software, sign_hardware
from insignia_interface import extract_engine, reattach_engine

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

installer_language_pattern = 'theme_{}.wxl'


class Config():

    class CodeSigning():
        def __init__(self, config):
            self.enabled = config['enabled']
            self.hardware = config['hardware']
            self.signtool_directory = config['signtool_directory']
            self.timestamp_server = config['timestamp_server']
            self.password = config['sign_password']
            self.certificate = config['certificate']

    def __init__(self, args):
        with open(args.config, 'r') as f:
            config = yaml.load(f)
        self.installer_cultures = config['installer_cultures']
        self.language_file = installer_language_pattern.format(config['installer_language'])
        self.wix_directory = config['wix_directory']
        self.output_file = os.path.join(
            args.output, config['servertool_distribution_name']) + '.exe'
        self.qml_directory = args.qml_dir
        self.vcredist_directory = os.path.join(config['vcredist_directory'], 'bin')
        self.sources_directory = args.sources_dir
        self.arch = config['arch']
        self.tmp_folder = os.path.join(self.sources_directory, engine_tmp_folder)
        self.code_signing = Config.CodeSigning(config['code_signing'])


def candle_and_light(
    config,
    project,
    output_file,
    components,
    candle_variables
):
    candle_output_folder = os.path.join(config.tmp_folder, project)
    candle(
        wix_directory=config.wix_directory,
        arch=config.arch,
        components=[os.path.join(config.sources_directory, c) for c in components],
        variables=candle_variables,
        extensions=wix_extensions,
        output_folder=candle_output_folder)

    light(
        working_directory=config.sources_directory,
        wix_directory=config.wix_directory,
        input_folder=candle_output_folder,
        installer_cultures=config.installer_cultures,
        installer_language=os.path.join(config.sources_directory, config.language_file),
        extensions=wix_extensions,
        output_file=output_file)


def sign(code_signing, target_file):
    if code_signing.enabled:
        if code_signing.hardware:
            sign_hardware(
                signtool_directory=code_signing.signtool_directory,
                target_file=target_file,
                timestamp_server=code_signing.timestamp_server)
        else:
            sign_software(
                signtool_directory=code_signing.signtool_directory,
                target_file=target_file,
                sign_password=code_signing.password,
                certificate=code_signing.certificate,
                timestamp_server=code_signing.timestamp_server)


def build_msi(
    config,
    project,
    output_file,
    components,
    candle_variables
):
    candle_and_light(
        config=config,
        project=project,
        output_file=output_file,
        components=components,
        candle_variables=candle_variables)
    sign(config.code_signing, output_file)


def build_exe(
    config,
    project,
    output_file,
    components,
    candle_variables
):
    candle_and_light(
        config=config,
        project=project,
        output_file=output_file,
        components=components,
        candle_variables=candle_variables)
    if config.code_signing.enabled:
        engine_filename = project + '.engine.exe'
        engine_path = os.path.join(config.tmp_folder, engine_filename)
        # Extract engine
        extract_engine(config.wix_directory, output_file, engine_path),
        # Sign it
        sign(config.code_signing, engine_path),
        # Reattach signed engine
        reattach_engine(config.wix_directory, engine_path, output_file, output_file),
        # Sign the bundle
        sign(config.code_signing, output_file)


def create_nxtool_installer(config):
    harvest_dir(
        wix_directory=config.wix_directory,
        source_dir=config.qml_directory,
        target_file=os.path.join(config.sources_directory, 'NxTool/NxToolQml.wxs'),
        component_group_name='QmlComponentGroup',
        directory_ref='NxToolDstQmlDir',
        source_dir_var='var.NxToolQmlDir')
    harvest_dir(
        wix_directory=config.wix_directory,
        source_dir=config.vcredist_directory,
        target_file=os.path.join(config.sources_directory, 'NxTool/NxToolVcrt14.wxs'),
        component_group_name='Vcrt14ComponentGroup',
        directory_ref='NxToolDstRootDir',
        source_dir_var='var.Vcrt14SrcDir')

    msi_filename = 'nxtool.msi'
    msi_file = os.path.join(config.tmp_folder, msi_filename)

    candle_msi_variables = {
        'Vcrt14SrcDir': config.vcredist_directory,
        'NxToolQmlDir': config.qml_directory
    }
    build_msi(
        config=config,
        project='nxtool-msi',
        output_file=msi_file,
        components=nxtool_components,
        candle_variables=candle_msi_variables)

    candle_exe_variables = {
        'NxtoolMsiName': msi_filename,
        'NxToolMsiFile': msi_file
    }
    build_exe(
        config=config,
        project='nxtool-exe',
        output_file=config.output_file,
        components=nxtool_exe_components,
        candle_variables=candle_exe_variables)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--qml-dir', help="Qml directory", required=True)
    parser.add_argument('--sources-dir', help="Sources directory", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()
    config = Config(args)
    create_nxtool_installer(config)


if __name__ == '__main__':
    main()
