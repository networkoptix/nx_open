'''
Options:
  --dir <directory>          Use directory instead of binary directory.
  --libdir <path>            Copy libraries to path.
  --plugindir <path>         Copy plugins to path.
  --debug                    Assume debug binaries.
  --release                  Assume release binaries.
  --pdb                      Deploy .pdb files (MSVC).
  --force                    Force updating files.
  --dry-run                  Simulation mode. Behave normally, but do not
                             copy/update any files.
  --no-plugins               Skip plugin deployment.
  --no-libraries             Skip library deployment.
  --qmldir <directory>       Scan for QML-imports starting from directory.
  --no-quick-import          Skip deployment of Qt Quick imports.
  --no-translations          Skip deployment of translations.
  --no-system-d3d-compiler   Skip deployment of the system D3D compiler.
  --compiler-runtime         Deploy compiler runtime (Desktop only).
  --no-compiler-runtime      Do not deploy compiler runtime (Desktop only).
  --webkit2                  Deployment of WebKit2 (web process).
  --no-webkit2               Skip deployment of WebKit2.
  --json                     Print to stdout in JSON format.
  --angle                    Force deployment of ANGLE.
  --no-angle                 Disable deployment of ANGLE.
  --no-opengl-sw             Do not deploy the software rasterizer library.
  --list <option>            Print only the names of the files copied.
                             Available options:
                              source:   absolute path of the source files
                              target:   absolute path of the target files
                              relative: paths of the target files, relative
                                        to the target directory
                              mapping:  outputs the source and the relative
                                        target, suitable for use within an
                                        Appx mapping file
  --verbose <level>          Verbose level.
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
