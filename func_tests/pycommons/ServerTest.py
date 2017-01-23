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
from FillStorage import fillStorage
from GenData import generateGuid

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
        self.config = RawConfigParser()
        self.config.optionxform = str
        self.guid = None

    def relativePath(self, fname):
        return os.path.join(self.tmpDir, fname)

    def prepare(self):
        if os.access(self.tmpDir, os.W_OK):
            Utils.removeDirTree(self.tmpDir)
        os.makedirs(self.tmpDir)

    def writeConfig(self, section=DEFAULT_SECTION, **kw):
        config_values = kw
        if section == DEFAULT_SECTION:
           config_values.setdefault(
             'systemName', FuncTest.str2fname(self.testName))
        set_config_section(
          self.config, section, values = config_values)
        f = file(self.cfgFilePath, 'w')
        self.config.write(f)
        f.close()

    @Utils.abstract
    def fillStorage(self, cameraMac, settings): pass

    @Utils.abstract
    def getDataDir(self): pass

    def setGuid(self, guid):
        self.guid = guid
    
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

    def getDataDir(self):
        return os.path.join(
            self.relativePath('data'),
            'data')

    def fillStorage(self, cameraMac, settings):
        return fillStorage(self.getDataDir(), cameraMac, settings)

class MediaServerBaseTest(FuncTest.FuncTestCaseBase, ComparisonMixin):

    class XTimedOut(Exception): pass

    def checkConnect(self, server, timeOut = DefaultTimeOut):
        start = time.time()
        address = server.address
        client = Client()
        while True:
            response = client.httpRequest(address, 'ec2/testConnection')
            if response.status != 200:
                remain = timeOut - (time.time() - start)
                if remain <= 0:
                    if response.status:
                        raise self.XTimedOut(
                          "Can't connect to server '%s': %d '%s'" % \
                          (address, response.status, response.error))
                    else:
                        raise self.XTimedOut(
                          "Can't connect to server '%s': got unexpected code '%s'" % \
                          (address, response.error))
            else:
                server.setGuid(response.data.get('ecsGuid'))
                return
            time.sleep(1.0)

    # Override the method in your derived test class,
    # if want to make specific server preparation
    @classmethod
    def prepareEnv(cls, server):
        server.prepare()

    # Override the method in your derived test class,
    # if need to change server config
    @classmethod
    def writeConfig(cls, server):
        server.writeConfig()

# Base class for testing single instance
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
        cls.prepareEnv(cls.server)
        cls.writeConfig(cls.server)
        FuncTest.tlog(10, "Start '%s' process" % cls.server.name)
        cls.server.start()

    def setUp(self):
        self.checkConnect(self.server)


# Base class for testing multiple instances
class MultiMediaServersTest(MediaServerBaseTest):

    count = 2
    servers = []

    @classmethod
    def globalFinalise(cls):
        for server in cls.servers:
            server.stop()
            FuncTest.tlog(10, "Stop '%s' process" % server.name)

    @classmethod
    def writeConfig(cls, server):
        server.writeConfig(
            serverGuid = generateGuid(),
            enableMultipleInstances = 1,
            guidIsHWID="no")

    @classmethod
    def globalInit(cls, config):
        super(MediaServerBaseTest, cls).globalInit(config)
        cdesc = cls.__name__
        for i in range(cls.count):
            server = MediaServerProcess(FuncTest.makeTestInfo(), cdesc)
            cls.prepareEnv(server)
            cls.writeConfig(server)
            cls.servers.append(server)
            FuncTest.tlog(10, "Start '%s' process" % server.name)
            server.start()

    def setUp(self):
        for server in self.servers:
            self.checkConnect(server)
