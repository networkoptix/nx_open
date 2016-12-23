# $Id$
# Artem V. Nikitin
# Server environment

import os, re

def _addDir2List(path, list):
    if path:
        list.append(os.path.dirname(path))

def get_path_parts(path):
    allparts = []
    while True:
        parts = os.path.split(path)
        if parts[0] == path:  # sentinel for absolute paths
            allparts.insert(0, parts[0])
            break
        elif parts[1] == path: # sentinel for relative paths
            allparts.insert(0, parts[1])
            break
        else:
            path = parts[0]
            allparts.insert(0, parts[1])
    return allparts

class ServerEnvironment:

    SrvEnvCfg = "../build_variables/target/current_config.py"
    BASE_PORT = 27000
    _srvBinPath = None
    _ldPaths = []
    _procNum = 0
    _portNum = 0

    @classmethod
    def _findPath(cls, path, pattern):
        for root, dirs, files in os.walk(path):
            for file in files:
                if re.match(pattern, file):
                    return os.path.join(root, file)
    @classmethod
    def _addDir2List(cls, path, list):
        if path:
            list.append(os.path.dirname(path))

    @classmethod
    def getBinPath(cls):
        return cls._srvBinPath

    @classmethod
    def getLDLibPath(cls):
        return ':'.join(cls._ldPaths)

    @classmethod
    def initEnv(cls):
        env = dict()
        execfile(cls.SrvEnvCfg, env)
        cls._ldPaths = [env['QT_LIB'], env['LIB_PATH']]
        cls._srvBinPath = env['BIN_PATH']

    @classmethod
    def getProcName(cls):
        cls._procNum+=1
        return "MediaServer_%d" % cls._procNum

    @classmethod
    def getPort(cls):
        cls._portNum+=1
        return cls.BASE_PORT + cls._portNum


ServerEnvironment.initEnv()
