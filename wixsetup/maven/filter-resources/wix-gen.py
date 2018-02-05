import environment
from heat_interface import harvest_dir

properties_dir = '${project.build.directory}'
bin_source_dir = '${bin_source_dir}'


def generate_client_backround():
    harvest_dir(
        '${ClientBgSourceDir}',
        'ClientBg.wxs',
        'ClientBgComponent',
        '${customization}BgDir',
        'var.ClientBgSourceDir')


def generate_fonts():
    harvest_dir(
        '${ClientFontsDir}',
        'ClientFonts.wxs',
        'ClientFontsComponent',
        'ClientFontsDir',
        'var.ClientFontsSourceDir')


def generate_help():
    clientHelpFile = 'ClientHelp.wxs'

    harvest_dir(
        '${ClientHelpSourceDir}',
        clientHelpFile,
        'ClientHelpComponent',
        '${customization}HelpDir',
        'var.ClientHelpSourceDir')

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
    harvest_dir(
        environment.client_qml_source_dir,
        'ClientQml.wxs',
        'ClientQmlComponent',
        'ClientQml',
        'var.ClientQmlDir')


def generate_client_vox():
    harvest_dir(
        '${VoxSourceDir}',
        'ClientVox.wxs',
        'ClientVoxComponent',
        '${customization}VoxDir',
        'var.VoxSourceDir')


def generate_server_vox():
    harvest_dir(
        '${VoxSourceDir}',
        'ServerVox.wxs',
        'ServerVoxComponent',
        '${customization}ServerVoxDir',
        'var.VoxSourceDir')


def generate_vcrt_14_client():
    harvest_dir(
        '${VC14RedistPath}/bin',
        'ClientVcrt14.wxs',
        'ClientVcrt14ComponentGroup',
        'ClientRootDir',
        'var.Vcrt14SrcDir')


def generate_vcrt_14_mediaserver():
    harvest_dir(
        '${VC14RedistPath}/bin',
        'ServerVcrt14.wxs',
        'ServerVcrt14ComponentGroup',
        'MediaServerRootDir',
        'var.Vcrt14SrcDir')


def generate_vcrt_14_traytool():
    harvest_dir(
        '${VC14RedistPath}/bin',
        'TraytoolVcrt14.wxs',
        'TraytoolVcrt14ComponentGroup',
        'TrayToolRootDir',
        'var.Vcrt14SrcDir')


def main():
    generate_client_backround()
    generate_fonts()
    generate_help()
    generate_client_qml()
    generate_client_vox()
    generate_server_vox()
    generate_vcrt_14_client()
    generate_vcrt_14_mediaserver()
    generate_vcrt_14_traytool()


if __name__ == '__main__':
    main()
