'''
Usage: windeployqt [options] [files]
Qt Deploy Tool 5.11.3

The simplest way to use windeployqt is to add the bin directory of your Qt
installation (e.g. <QT_DIR\bin>) to the PATH variable and then run:
  windeployqt <path-to-app-binary>
If ICU, ANGLE, etc. are not in the bin directory, they need to be in the PATH
variable. If your application uses Qt Quick, run:
  windeployqt --qmldir <path-to-app-qml-files> <path-to-app-binary>

Options:
  -?, -h, --help            Displays this help.
  -v, --version             Displays version information.
  --dir <directory>         Use directory instead of binary directory.
  --libdir <path>           Copy libraries to path.
  --plugindir <path>        Copy plugins to path.
  --debug                   Assume debug binaries.
  --release                 Assume release binaries.
  --pdb                     Deploy .pdb files (MSVC).
  --force                   Force updating files.
  --dry-run                 Simulation mode. Behave normally, but do not
                            copy/update any files.
  --no-patchqt              Do not patch the Qt5Core library.
  --no-plugins              Skip plugin deployment.
  --no-libraries            Skip library deployment.
  --qmldir <directory>      Scan for QML-imports starting from directory.
  --no-quick-import         Skip deployment of Qt Quick imports.
  --no-translations         Skip deployment of translations.
  --no-system-d3d-compiler  Skip deployment of the system D3D compiler.
  --compiler-runtime        Deploy compiler runtime (Desktop only).
  --no-compiler-runtime     Do not deploy compiler runtime (Desktop only).
  --webkit2                 Deployment of WebKit2 (web process).
  --no-webkit2              Skip deployment of WebKit2.
  --json                    Print to stdout in JSON format.
  --angle                   Force deployment of ANGLE.
  --no-angle                Disable deployment of ANGLE.
  --no-opengl-sw            Do not deploy the software rasterizer library.
  --list <option>           Print only the names of the files copied.
                            Available options:
                             source:   absolute path of the source files
                             target:   absolute path of the target files
                             relative: paths of the target files, relative
                                       to the target directory
                             mapping:  outputs the source and the relative
                                       target, suitable for use within an
                                       Appx mapping file
  --verbose <level>         Verbose level (0-2).

Qt libraries can be added by passing their name (-xml) or removed by passing
the name prepended by --no- (--no-xml). Available libraries:
bluetooth concurrent core declarative designer designercomponents enginio
gamepad gui qthelp multimedia multimediawidgets multimediaquick network nfc
opengl positioning printsupport qml qmltooling quick quickparticles quickwidgets
script scripttools sensors serialport sql svg test webkit webkitwidgets
websockets widgets winextras xml xmlpatterns webenginecore webengine
webenginewidgets 3dcore 3drenderer 3dquick 3dquickrenderer 3dinput 3danimation
3dextras geoservices webchannel texttospeech serialbus webview

Arguments:
  [files]                   Binaries or directory containing the binary.
'''


import os
from environment import execute_command

def windeployqt_executable(qt_directory):
    return os.path.join(qt_directory, 'bin', 'windeployqt.exe') # '{0}/bin/windeployqt.exe'.format(qt_directory)

def common_windeployqt_options():
    return ['--no-translations', '--force', '--no-libraries', '--no-plugins']

def deploy_qt_command(qt_directory, executable, qml_dir, target_dir):
    command = [windeployqt_executable(qt_directory), executable, '--qmldir', qml_dir, '--dir', target_dir]
    command += common_windeployqt_options()
    return command

def deploy_qt(qt_directory, executable, qml_dir, target_dir):
    execute_command(deploy_qt_command(qt_directory, executable, qml_dir, target_dir), True)

def cleanup_qtwebprocess(target_dir):
    webprocess_path = os.path.join(target_dir, 'QtWebProcess.exe')
    if os.path.exists(webprocess_path):
        os.unlink(webprocess_path)
