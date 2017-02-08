# $Id$
# Artem V. Nikitin
# base classes for functional tests

import subprocess, unittest, os, string
from Logger import log, makePrefix, LOGLEVEL

class XTimedOut(Exception): pass

VSSH_CMD = "./vssh-ip.sh"
MEDIA_SERVER_DIR='/opt/networkoptix/mediaserver'

# test logging
def tlog(level, msg):
    log(level, 'TEST: %s' % msg)

def execVBoxCmd(host, *command):
    cmd = ' '.join(command)
    try:
        tlog(LOGLEVEL.DEBUG + 4, "DEBUG: %s" % str([VSSH_CMD, host, 'sudo'] + list(command)))
        output = subprocess.check_output(
          [VSSH_CMD, host, 'sudo'] + list(command),
          shell=False, stderr=subprocess.STDOUT)
        tlog(LOGLEVEL.DEBUG + 4, "%s command '%s':\n%s" % (host, cmd, output))
    except subprocess.CalledProcessError, x:
        tlog(LOGLEVEL.DEBUG + 4, "%s command '%s' failed, code=%d:\n%s" % \
             (host, cmd, x.returncode, x.output))
        raise

class TestInfo:

    def __init__( self, workDir, tmpDir ):
        self.workDir   = workDir  # working dir for servers
        self.tmpDir    = tmpDir

def str2fname( name ):
  for ch in ' :,|><;+&#@*^%[]\/"()':
    name = string.replace(name, ch, '_')
  return name

def makeTestInfo( workDir = '.' ):
    tmpDir = os.path.join(workDir, 'output')
    return TestInfo(workDir, tmpDir)

class FuncTestCaseBase(unittest.TestCase):

    helpStr = "No help provided for this test"
    _suites = ()
    _init_suites_done = False
    _test_name = '<UNNAMED!>'
    _test_key = 'NONE'  # a name to pass as an argument to commands

    @classmethod
    def globalInit(cls, config):
        if config is None:
            raise FuncTestError("%s can't be configured, config is None!" % cls.__name__)
        cls.config = config
        cls.init_suites()

    @classmethod
    def globalFinalise(cls):
        raise NotImplementedError()

    @classmethod
    def isFailFast(cls, suit_name=""):
        # it could depend on the specific suit
        return True

    ################################################################################
    # These 3 methods used in a caller (see the RunTests and functest.CallTest funcions)
    @classmethod
    def _check_suites(cls):
        if not cls._suites:
            raise RuntimeError("%s's test suites list is empty!" % cls.__name__)

    @classmethod
    def iter_suites(cls):
        cls._check_suites()
        return (s[0] for s in cls._suites)

    @classmethod
    def init_suites(cls):
        "Called by RunTests, prepares attributes with suits names contaning test cases names"
        if cls._init_suites_done:
            return
        cls._check_suites()
        for name, tests in cls._suites:
            if hasattr(cls, name):
                raise AssertionError("Test suite naming error: class %s already has attrinute %s" % (cls.__name__, name))
            setattr(cls, name, tests)
        cls._init_suites_done = True

    ################################################################################

    def setUp(self):
        if pycommons.Logger.logger.isSystem():
            print  # this is because the unittest module desn't add \n after printing new test name,
            #  so the first log message would be printed in the same line
    #    print "*** Setting up: %s" % self._testMethodName  # may be used for debug ;)
    ####################################################

    @classmethod
    def filterSuites(cls, *names):
        return tuple(sw for sw in cls._suites if sw[0] in names)

        
