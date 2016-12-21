# $Id$
# Artem V. Nikitin
# Base classes for single media-server tests

import os, FuncTest, Utils, ProcMgr, time
from ConfigParser import RawConfigParser
from ServerEnvironment import ServerEnvironment
from ServerConfig import CfgFileName, RuntimeCfgFileName, set_config_section
from Config import DEFAULT_SECTION
from ComparisonMixin import ComparisonMixin
from MockClient import DigestAuthClient as Client

DefaultTimeOut = 5.0

class MediaServerBase:

    def __init__(self, info, testName):
        self.info = info
        self.testName = testName  # test class desc
        self.name = ServerEnvironment.getProcName()
        self.tmpDir = os.path.join(
          os.path.abspath(info.tmpDir),
          FuncTest.str2fname(testName),
          self.name)
        self.cfgFilePath = self.relativePath(CfgFileName)

    def relativePath(self, fname):
        return os.path.join(self.tmpDir, fname)

    def prepare(self):
        if os.access(self.tmpDir, os.W_OK):
            Utils.removeDirTree(self.tmpDir)
        os.makedirs(self.tmpDir)

    def writeConfig(self, section=DEFAULT_SECTION, **kw):
        config = RawConfigParser()
        config.optionxform = str
        config_values = kw
        if section == DEFAULT_SECTION:
            config_values.setdefault(
              'systemName', FuncTest.str2fname(self.testName))
        set_config_section(
          config, section, values = config_values)
        f = file(self.cfgFilePath, 'w')
        config.write(f)
        f.close()

class MediaServerProcess(MediaServerBase, ProcMgr.Process):

    SrvName = 'mediaserver'

    def __init__( self, info, testName ):
        MediaServerBase.__init__(self, info, testName)
        server_env = os.environ.copy()
        server_env['LD_LIBRARY_PATH'] = ServerEnvironment.getLDLibPath()
        ProcMgr.Process.__init__(self, self.name, self._getCommand(), env = server_env )
        self.port = ServerEnvironment.getPort()

    def writeConfig( self, section=DEFAULT_SECTION, **kw ):
        if section == DEFAULT_SECTION:
            config_values = {
                'dataDir': self.relativePath('data'),
                'logLevel': 'DEBUG',
                'port': self.port }
            config_values.update(kw)
        else:
            config_values = kw
        MediaServerBase.writeConfig(self, section, **config_values)

    @property
    def address(self):
        return 'localhost:%d' % self.port

    def prepare(self):
        if os.access(self.tmpDir, os.W_OK):
            Utils.removeDirTree(self.tmpDir)
        os.makedirs(self.tmpDir)

    def _getCommand(self):
        runtimeFilePath = self.relativePath(RuntimeCfgFileName)
        return ['%s' % os.path.join(ServerEnvironment.getBinPath(), self.SrvName) ,
                '-e', '--conf-file=%s' % self.cfgFilePath,
                '--runtime-conf-file=%s' % runtimeFilePath]

class MediaServerBaseTest(FuncTest.FuncTestCaseBase, ComparisonMixin):

    class XTimedOut(Exception): pass

    def checkConnect(self, address, timeOut = DefaultTimeOut):
        start = time.time()
        client = Client()
        while True:
            response = client.httpRequest(address, 'ec2/testConnection')
            if response.status != 200:
                remain = timeOut - (time.time() - start)
                if remain <= 0:
                    if response.error:
                        raise self.XTimedOut(
                          "Can't connect to server '%s': %s" % \
                          (address, response.error))
                    else:
                        raise self.XTimedOut(
                          "Can't connect to server '%s': got unexpected code %d" % \
                          (address, response.error))
            else:
                self.guid = response.data['ecsGuid']
                return
            time.sleep(1.0)

    # Override the method in your dirrived test class,
    # if want to make specific server preparation
    def prepareEnv(self, server):
        server.prepare()

    # Override the method in your dirrived test class,
    # if need to change server config
    def writeConfig(self, server):
        server.writeConfig()

class MediaServerInstanceTest(MediaServerBaseTest):

    server = None

    @classmethod
    def globalFinalise(cls):
        if cls.server:
            cls.server.stop()
            FuncTest.tlog(10, "Stop '%s' process" % cls.server.name)

    @classmethod
    def globalInit(cls, config):
        super(MediaServerBaseTest, cls).globalInit(config)
        cdesc = cls.__name__
        cls.server = MediaServerProcess(FuncTest.makeTestInfo(), cdesc)

    def setUp(self):
        FuncTest.tlog(10, "Start '%s' process" % self.server.name)
        self.prepareEnv(self.server)
        self.writeConfig(self.server)
        self.server.start()
        self.checkConnect(self.server.address)
