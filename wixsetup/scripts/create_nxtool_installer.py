#!/bin/python2

import argparse
import os
import yaml

from heat_interface import harvest_dir
from light_interface import light
from candle_interface import candle
from signtool_interface import sign
from insignia_interface import extract_engine, reattach_engine

engine_tmp_folder = 'obj'

wix_extensions = [
    'WixFirewallExtension',
    'WixUtilExtension',
    'WixUIExtension',
    'WixBalExtensionExt']

nxtool_components = [
    'NxtoolDlg',
    'Nxtool',
    'NxtoolQuickControls',
    'NxtoolVcrt14',
    'MyExitDialog',
    'UpgradeDlg',
    'SelectionWarning',
    'Product-nxtool']

nxtool_exe_components = [
    'ArchCheck',
    'NxtoolPackage',
    'Product-nxtool-exe']


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


def create_nxtool_installer(
    wix_directory,
    qml_directory,
    vcredist_directory
):
    harvest_dir(
        wix_directory=wix_directory,
        source_dir=qml_directory,
        target_file='NxToolQml.wxs',
        component_group_name='NxToolQmlComponent',
        directory_ref='NxToolQml',
        source_dir_var='var.NxToolQmlDir')
#    harvest_dir(
#        wix_directory,
#        '${NxtoolQuickControlsDir}',
#        'NxtoolQuickControls.wxs',
#        'NxtoolQuickControlsComponent',
#        'QtQuickControls',
#        'var.NxtoolQuickControlsDir')
    harvest_dir(
        wix_directory=wix_directory,
        source_dir=vcredist_directory,
        target_file='NxToolVcrt14.wxs',
        component_group_name='NxToolVcrt14ComponentGroup',
        directory_ref='NxToolRootDir',
        source_dir_var='var.Vcrt14SrcDir')

    candle_msi_variables = {
        'NxtoolVcrt14DstDir': '${customization}NxtoolDir',
        'NxtoolQuickControlsDir': '${NxtoolQuickControlsDir}',
        'NxtoolQmlDir': '${project.build.directory}/nxtoolqml'
    }
    build_msi('nxtool-msi', nxtool_msi_file, nxtool_components, candle_msi_variables)

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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--qml-dir', help="Qml directory", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    output_file = os.path.join(
        args.output, config['servertool_distribution_name']) + '.msi'
    create_nxtool_installer(
        wix_directory=config['wix_directory'],
        qml_directory=args.qml_dir,
        vcredist_directory=config['vcredist_directory'])


if __name__ == '__main__':
    main()
