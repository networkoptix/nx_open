import io, sys, os, platform
import ConfigParser
import argparse
#from main import get_environment_variable, cd

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--cfg", help="configuration, default - release or environment variable <configuration>")
    parser.add_argument("--cst", help="customization, default - HD Witness or environment variable <customization>")
    parser.add_argument("--arch", help="build architecture, default - python executable architecture")

    args = parser.parse_args()    
    if args.cfg:
        build_configuration = args.cfg
    elif os.getenv('configuration'):
        build_configuration = os.getenv('configuration')
    else:
        build_configuration = 'release'
        
    if args.cst:
        build_customization = args.cst
    elif os.getenv('customization'):
        build_customization = os.getenv('customization')
    else:
        build_customization = 'default'

    if args.arch:
        build_arch = args.cst
    elif os.getenv('customization'):
        build_arch = os.getenv('customization')
    else:
        build_arch = 'default'        
        
    f = open('configure_settings_tmp.py', 'w')
    print >> f, \
    'customization = "%s" \nconfiguration = "%s" \nbuild_arch = "%s"' %(build_customization, build_configuration, build_arch)

    print '\n++++++++++++++++++++++++ CONFUGURED ++++++++++++++++++++++++\n'
    print >> sys.stderr, 'customization is: %s' % build_customization
    print >> sys.stderr, 'configuration is: %s' % build_configuration
    print >> sys.stderr, 'build_arch is: %s' % build_arch