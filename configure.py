import io, sys, os, platform
import ConfigParser
import argparse
#from main import get_environment_variable, cd

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--cfg", help="configuration, default - release")
    parser.add_argument("--cst", help="customization, default - HD Witness")
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

    zf = open('configure_settings_tmp.py', 'w')
    print >> f, \
    'customization = "%s" \nconfiguration = "%s"' %(build_customization, build_configuration)

    print '\n++++++++++++++++++++++++ CONFUGURED ++++++++++++++++++++++++\n'
    print >> sys.stderr, 'customization is: %s' % build_customization
    print >> sys.stderr, 'configuration: is %s' % build_configuration