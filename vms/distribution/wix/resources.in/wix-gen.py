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


def generate_vox():
    harvest_dir(
        '${VoxSourceDir}',
        'vox.wxs',
        'vox',
        'vox',
        'var.VoxSourceDir')


def main():
    generate_client_backround()
    generate_fonts()
    generate_help()
    generate_client_qml()
    generate_vox()


if __name__ == '__main__':
    main()
