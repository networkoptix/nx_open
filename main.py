import io, sys, os, platform
import ConfigParser
import argparse
from os.path import dirname, join
    
#class Config:
#    pass

class cd:         
    def __init__(self, newPath):
        self.newPath = newPath

    def __enter__(self):
        self.savedPath = os.getcwd()
        os.chdir(self.newPath)

    def __exit__(self, etype, value, traceback):
        os.chdir(self.savedPath)
    
def get_environment_variable(variable):
    try:
        with open(join(dirname(os.path.abspath(__file__)),'configure_settings_tmp.py')): from configure_settings_tmp import customization, configuration
    except IOError:
        print 'Please run configure.py first'
        sys.exit(1)
    
    if variable == 'platform':
        if sys.platform == 'win32':
            return 'windows'
        elif sys.platform == 'linux2':
            return 'linux'
        elif sys.platform == 'darwin':
            return 'macosx'   
            
    elif variable == 'arch':        
        if platform.architecture()[0] == '64bit':
            return 'x64'
        else:
            return 'x86'        
    
    elif variable == 'customization':
        return customization
    
    elif variable == 'configuration':
        return configuration
        
    else:        
        if os.getenv(variable):
            return os.getenv(variable)
        else:
            config = ConfigParser.RawConfigParser(allow_no_value=True)
            config.readfp(open(os.path.dirname(os.path.abspath(__file__)) + '/build-' + customization + '.properties'))
            return config.get("basic", variable)  

def get_executable_extension():
    if get_environment_variable('platform') == 'windows':
        return 'bat'
    else:
        return 'sh'            