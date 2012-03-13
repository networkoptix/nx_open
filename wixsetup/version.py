import os

ORGANIZATION_NAME    = 'Network Optix'
APPLICATION_NAME     = 'HD Witness'
APPLICATION_VERSION  = '0.9.3'
BUILD_NUMBER = os.getenv('BUILD_NUMBER', 'dev')

def set_env():
    os.putenv('ORGANIZATION_NAME', ORGANIZATION_NAME)
    os.putenv('APPLICATION_NAME', APPLICATION_NAME)
    os.putenv('APPLICATION_VERSION', APPLICATION_VERSION)
    os.putenv('BUILD_NUMBER', BUILD_NUMBER)

def create_bat():
    f = open('version.bat', 'w')
    print >> f, 'SET "ORGANIZATION_NAME=%s"' % ORGANIZATION_NAME
    print >> f, 'SET "APPLICATION_NAME=%s"' % APPLICATION_NAME
    print >> f, 'SET "APPLICATION_VERSION=%s"' % APPLICATION_VERSION
    print >> f, 'SET "BUILD_NUMBER=%s"'% BUILD_NUMBER
    
if __name__ == '__main__':
    set_env()
    create_bat()