import os, sys, subprocess, shutil
from subprocess import Popen, PIPE, STDOUT
from os.path import dirname, join, exists, isfile
from heat_interface import harvest_dir
from windeployqt_interface import deploy_qt, cleanup_qtwebprocess

properties_dir = '${project.build.directory}'
has_nxtool = ('${nxtool}' == 'true')
bin_source_dir = '${bin_source_dir}'

def generate_client_backround():
    harvest_dir(
        '${ClientBgSourceDir}',
        'ClientBg.wxs',
        'ClientBgComponent',
        '${customization}BgDir',
        ['var.ClientBgSourceDir'])

def generate_fonts():
    harvest_dir(
        '${ClientFontsDir}',
        'ClientFonts.wxs',
        'ClientFontsComponent',
        'ClientFontsDir',
        ['var.ClientFontsDir'])

def generate_help():
    clientHelpFile = 'ClientHelp.wxs'

    harvest_dir(
        '${ClientHelpSourceDir}',
        clientHelpFile,
        'ClientHelpComponent',
        '${customization}HelpDir',
        ['var.ClientHelpSourceDir'])

    fragments = (
        (r'''Id="jquery.js"''', '''Id="skawixjquery.js"'''),
        (r'''Id="index.html"''', '''Id="skawixindex.html"'''),
		(r'''Id="calendar_checked.png"''', '''Id="skawixcalendar_checked.png"'''),
		(r'''Id="live_checked.png"''', '''Id="skawixlive_checked.png"'''),
        )

    text = open(clientHelpFile, 'r').read()
    for xfrom, xto in fragments:
        text = text.replace(xfrom, xto)
    open(clientHelpFile, 'w').write(text)

def generate_client_qml():
    qmldir = '${root.dir}/client/nx_client_desktop/static-resources/qml/'
    client_executable = '{0}/desktop_client.exe'.format(bin_source_dir)
    deploy_qt(client_executable, qmldir, 'clientqml')
    cleanup_qtwebprocess('clientqml')
    harvest_dir(
        'clientqml',
        'ClientQml.wxs',
        'ClientQmlComponent',
        'ClientQml',
        ['var.ClientQmlDir'])

def generate_nxtool_qml():
    qmldir_nxtool = '${root.dir}/nxtool/static-resources/src/qml'
    nxtool_executable = '{0}/nxtool.exe'.format(bin_source_dir)
    deploy_qt(nxtool_executable, qmldir_nxtool, 'nxtoolqml')
    cleanup_qtwebprocess('nxtoolqml')
    harvest_dir(
        'nxtoolqml',
        'NxtoolQml.wxs',
        'NxtoolQmlComponent',
        'NxtoolQml',
        ['var.NxtoolQmlDir'])
    harvest_dir(
        '${NxtoolQuickControlsDir}',
        'NxtoolQuickControls.wxs',
        'NxtoolQuickControlsComponent',
        'QtQuickControls',
        ['var.NxtoolQuickControlsDir'])

def generate_client_vox():
    harvest_dir(
        '${ClientVoxSourceDir}',
        'ClientVox.wxs',
        'ClientVoxComponent',
        '${customization}VoxDir',
        ['var.ClientVoxSourceDir'])

def generate_server_vox():
    harvest_dir(
        '${ClientVoxSourceDir}',
        'ServerVox.wxs',
        'ServerVoxComponent',
        '${customization}ServerVoxDir',
        ['var.ClientVoxSourceDir'])

def generate_vcrt_14(component):
    target_file = '{}Vcrt14.wxs'.format(component)

    harvest_dir(
        '${VC14RedistPath}/bin',
        target_file,
        '{}Vcrt14ComponentGroup'.format(component),
        '{}Vcrt14DstDir'.format(component),
        ['var.Vcrt14SrcDir'])

    # Add component_ prefix to avoid duplicating IDs for Server/Traytool
    lines = []
    with open(target_file, 'r') as f:
        for line in f:
            lines.append(line
                        .replace('Component Id="', 'Component Id="{}_'.format(component))
                        .replace('ComponentRef Id="', 'ComponentRef Id="{}_'.format(component))
                        .replace('<File Id="', '<File Id="{}_'.format(component)))

    with open(target_file, 'w') as f:
        f.write(''.join(lines))

def main():
    generate_client_backround()
    generate_fonts()
    generate_help()
    generate_client_qml()
    generate_client_vox()
    generate_server_vox()
    if has_nxtool:
        generate_nxtool_qml()
    # TODO: Paxton if win86, Nxtool if has_nxtool
    for component in ['Client', 'Server', 'Traytool', 'Paxton', 'Nxtool']:
        generate_vcrt_14(component)

if __name__ == '__main__':
    main()
