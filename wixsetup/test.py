import os

def create_bat():
    f = open('version.bat', 'w')
    print >> f, 'SET "ORGANIZATION_NAME=%s"' % ORGANIZATION_NAME
    print >> f, 'SET "APPLICATION_NAME=%s"' % APPLICATION_NAME
    print >> f, 'SET "APPLICATION_VERSION=%s"' % APPLICATION_VERSION
    print >> f, 'SET "BUILD_NUMBER=%s"'% BUILD_NUMBER
    
