import io, sys, os, platform
import ConfigParser
import argparse
import subprocess
from main import get_environment_variable, cd
from os.path import dirname, join
import re

def runProcess(exe): 
    p = subprocess.Popen(exe, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    while(True):
      retcode = p.poll() #returns None while subprocess is running
      line = p.stdout.readline()
      yield line
      if(retcode is not None):
        break

def donotrebuild(result):
    if result == 'true':
        os.makedirs('build_variables/target')
        f = open('build_variables/target/donotrebuild', 'w')
        print 'build_variables/target/donotrebuild'
        f.close()
        print "++++++++++++++++++++++++ Reconfigure is NOT required ++++++++++++++++++++++++"
    else:
        print "++++++++++++++++++++++++ Reconfigure is required ++++++++++++++++++++++++"
        if os.path.isfile('build_variables/target/donotrebuild'):
            os.unlink('build_variables/target/donotrebuild')
        if os.path.isfile('build_environment/target/donotrebuild'):    
            os.unlink('build_environment/target/donotrebuild')             
        
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
        build_arch = args.arch
    else:
        get_environment_variable('arch')        

    try:
        with open(join(dirname(os.path.abspath(__file__)),'configure_settings_tmp.py')): 
            from configure_settings_tmp import branch
            print >> sys.stderr, 'old branch is: %s' % branch   
    except IOError:
        print 'not configured'
        branch = ""

    build_branch = os.popen('hg branch').read()[:-1]
    
    print '\n++++++++++++++++++++++++ CONFUGURED ++++++++++++++++++++++++\n'
    print >> sys.stderr, 'customization is: %s' % build_customization
    print >> sys.stderr, 'configuration is: %s' % build_configuration
    print >> sys.stderr, 'build_arch is: %s' % build_arch
    print >> sys.stderr, 'branch is: %s\n' % build_branch
    if get_environment_variable('platform') == 'windows':
        if branch == build_branch and os.path.isfile('build_environment/target/donotrebuild'):
            donotrebuild('true')
        else:
            donotrebuild('false')        
    else:
        if arch == build_arch and box == build.box and branch == build_branch and os.path.isfile('build_environment/target/donotrebuild'):
            donotrebuild('true')
        else:
            donotrebuild('false')    
    f = open('configure_settings_tmp.py', 'w')
    print >> f, \
    'customization = "%s" \nconfiguration = "%s" \nbuild_arch = "%s" \nbranch = "%s"' %(build_customization, build_configuration, build_arch, build_branch)
    f.close()            