# $Id$
# Artem V. Nikitin
# Functests configuration

import os
from ConfigParser import RawConfigParser
from ProxyObj import ProxyObj

DEFAULT_CONFIG_FILE_NAME="functest.cfg"
DEFAULT_SECTION="General"
DEFAULT_TIMEOUT=2.0

class Config(ProxyObj):

    def __init__(self, *args, **kw_args):
        self.__config = RawConfigParser(*args, **kw_args)

    def _configObjCall( self, fn, section, option, default=None):
        if not self.has_option(section, option):
            return default
        return fn(section, option)

    def _proxyObjCall( self, method, section, option, default=None ):
        fn = getattr(self.__config, method)
        assert callable(fn), fn  # try to deny access to data members - at least to not callable ones
        return self._configObjCall(fn, section, option, default)

    def read(self, configfile = None):
        return self.__config.read(configfile or DEFAULT_CONFIG_FILE_NAME)

    def __getattr__( self, name ):
        if name and name[-5:] == '_safe':
            method = name[:-5]
            return self.ProxyMethod(self, method)
        return getattr(self.__config, name)

    @property
    def clusterTestSleepTime(self):
        return self.getint_safe(
          DEFAULT_SECTION,
          "clusterTestSleepTime",
          DEFAULT_TIMEOUT)

# global test config
config = Config()

def get_servers( testName = None ):
    srvlist = config.get(DEFAULT_SECTION, "serverList")
    if testName and config.has_section(testName):
        srvlist =  config.get_safe(testName, "serverList", srvlist)
    return srvlist.split(",")
