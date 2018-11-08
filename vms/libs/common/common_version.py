import os, sys, re
from convert import platform, setup_ffmpeg

ORGANIZATION_NAME    = 'Network Optix'
APPLICATION_VERSION  = '1.4.0'
BUILD_NUMBER = os.getenv('BUILD_NUMBER', '0')

def get_library_path():
    if platform() == 'mac':
        return os.getenv('DYLD_LIBRARY_PATH')
    
    return None

def set_library_path(value):
    if platform() == 'mac':
        os.putenv('DYLD_LIBRARY_PATH', value)
    elif platform() == 'linux':
        os.putenv('LD_LIBRARY_PATH', value)

def set_env():
    REVISION = os.popen('hg id -i').read().strip()

    ffmpeg_path, ffmpeg_path_debug, ffmpeg_path_release = setup_ffmpeg()
    library_path = get_library_path()
    set_library_path(os.path.join(ffmpeg_path_release, 'lib'))
    ffmpeg_output = os.popen(ffmpeg_path_release + '/bin/ffmpeg -version').read()
    if library_path:
        set_library_path(library_path)
    
    FFMPEG_VERSION = re.search(r'(N-[^\s]*)', ffmpeg_output).groups()[0]
    
    os.putenv('ORGANIZATION_NAME', ORGANIZATION_NAME)
    os.putenv('APPLICATION_VERSION', APPLICATION_VERSION)
    os.putenv('BUILD_NUMBER', BUILD_NUMBER)
    os.putenv('REVISION', REVISION)
    os.putenv('FFMPEG_VERSION', FFMPEG_VERSION)
    return ORGANIZATION_NAME, APPLICATION_VERSION, BUILD_NUMBER, REVISION, FFMPEG_VERSION

if __name__ == '__main__':
    print APPLICATION_VERSION
