import io, sys, os, platform
import ConfigParser
import argparse

class Config:
    pass
    
def get_build_environment():   
    config = Config()
    parser = argparse.ArgumentParser()
    parser.add_argument("--cfg", help="configuration, default - release")
    parser.add_argument("--cst", help="customization, default - HD Witness")
    args = parser.parse_args()
    if platform.architecture()[0] == '64bit':
        config.build_arch = 'x64'
    else:
        config.build_arch = 'x86'
        
    if args.cfg:
        config.build_configuration = args.cfg
    elif os.getenv('configuration'):
        config.build_configuration = os.getenv('configuration')
    else:
        config.build_configuration = 'release'
        
    if args.cst:
        config.build_customization = args.cst
    elif os.getenv('customization'):
        config.build_customization = os.getenv('customization')
    else:
        config.build_customization = 'default'        

    if sys.platform == 'win32':
        config.build_platform = 'windows'
    elif sys.platform == 'linux2':
        config.build_platform = 'linux'
    elif sys.platform == 'darwin':
        config.build_platform = 'macosx'    
        
    return config    
    
def get_environment_variable(variable):
    if variable == 'platform':
        return get_os_name()
    elif os.getenv(variable):
        return os.getenv(variable)
    else:
        config = ConfigParser.RawConfigParser(allow_no_value=True)
        config.readfp(open(os.path.dirname(os.path.abspath(__file__)) + '/build-' + get_build_environment().build_customization + '.properties'))
        return config.get("basic", variable)

config = get_build_environment()        
print >> sys.stderr, 'Python arch is %s' % config.build_arch
print >> sys.stderr, 'platform is %s' % config.build_platform
print >> sys.stderr, 'customization is %s' % config.build_customization
print >> sys.stderr, 'product.name is %s' % get_environment_variable('product.name')
print >> sys.stderr, 'build.configuration is %s' % config.build_configuration
print >> sys.stderr, 'environment is %s' % get_environment_variable('environment')
