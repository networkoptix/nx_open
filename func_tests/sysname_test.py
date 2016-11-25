# -*- coding: utf-8 -*-
__author__ = 'Danil Lavrentyuk'
""" The system name test checks all mediaservers report the same systemName.
Then it tries to change systemName of each mediaserver and checks
if that name is changed and that the server is no more in the same
system with the others.
"""
import sys
import time, traceback
import urllib, urllib2
import pprint
import uuid
import json
from pycommons.Logger import log, LOGLEVEL

#from functest_util import SafeJsonLoads
#from generator import BasicGenerator

# It was SystemNameTest earlier ...
class SystemIdTest(object):
    _oldSystemId = None
    _syncTime = 2

    def __init__(self, config):
        self._idsUsed = set()
        self._guidDict = dict()
        self._serverList = []
        self._config = config
        self._idsChanged = False
        self._serverList = self._config.rtget("ServerList")
        self._syncTime = self._config.rtget("SleepTime")

    def _doGet(self, addr, methodName):
        url = "http://%s/ec2/%s" % (addr,methodName)
        log(LOGLEVEL.DEBUG + 9, "Connection to " + url)
        try:
            response = urllib2.urlopen(url)
            assert response.getcode() == 200, "Failed request to %s: HTTP Error %s" % (
                url, response.getcode()
            )
            return response.read()
        except Exception as err:
            assert False, "Failed request %s: exception: %s, %s" % (url,) + sys.exc_info()[0:2]

    def _changeSystemId(self, addr, _id):
        url = "http://%s/api/configure?%s" % (addr,urllib.urlencode({"localSystemId": _id}))
        log(LOGLEVEL.DEBUG + 9, "Request:%s" % url)
        try:
            response = urllib2.urlopen(url)
            assert response.getcode() == 200, "Failed to set localSysteId: HTTP Error %s" % response.getcode()
        except Exception as err:
            assert False, "Failed to set localSysteId: %s" % (err,)

    def _ensureServerSystemId(self, end=False):
        systemId = self._oldSystemId if end else None
        for s in self._serverList:
            obj = self._config.rtget("ServerObjs")[s]
            #print "Server %s, obj: %s" % (s, pprint.pformat(obj))
            if systemId is None:
                systemId = obj["localSystemId"]
            else:
                assert systemId == obj["localSystemId"], (
                    "%s%s has localSystemId %s different from others: %s"
                     % (("After rollback " if end else ""), s, obj["localSystemId"], systemId))

            self._guidDict[s] = obj["ecsGuid"]
        if not end:
            self._oldSystemId = systemId
            self._idsUsed.add(systemId)

    def _newSystemId(self):
        while True:
            newId = '{%s}' % uuid.uuid4()
            if newId not in self._idsUsed:
                self._idsUsed.add(newId)
                return newId

    def _ensureSystemIdsDiffer(self, serverId, newSysId):
        for s in self._serverList:
            anser = self._doGet(s, 'testConnection')
            try:
                data = json.loads(anser)
            except ValueError as err:
                assert False, "%s returned wrong answer on ec2/testConnection request: '%s'" % (s, anser)
            if data['ecsGuid'] == serverId:
                assert data['localSystemId'] == newSysId, (
                    "%s reported localSystemId %s different from assigned in test (%s)" %
                    (s, data['localSystemId'], newSysId)
                )
            else:
                assert data['localSystemId'] != newSysId, (
                    "%s reported localSystemId %s which was assigned to different server" %
                    (s, data['localSystemId'])
                )

    def _doSingleTest(self, s):
        thisGUID = self._guidDict[s]
        newId = self._newSystemId()
        self._changeSystemId(s, newId)
        self._idsChanged = True
        # wait for the time to sync
        time.sleep(self._syncTime)
        # issue a getMediaServerEx request to test whether all the servers in
        # the list has the expected offline/online status
        ret = self._doGet(s, "getMediaServersEx")
        try:
            obj = json.loads(ret)
        except ValueError as err:
            assert False, "The server %s sends wrong response to getMediaServersEx: %s" % (s, ret)
        # After changing system name the server shouldn't forget of the another, but should think
        # the another server is offline (since they aren't in the same system).
        for ele in obj:
            #pprint.pprint(obj)
            if ele["id"] == thisGUID:
                assert ele["status"] == "Online", (
                    "The server %s with GUID %s should be Online" % (s,thisGUID))
                #if ele["systemName"] != newName:
                #    return (False,"The server %s with GUID %s hasn't changed its systemName!" % (s,thisGUID))
            else:
                assert ele["status"] == "Offline", (
                    "The server %s with GUID %s should be Offline when checked on Server: %s" %
                    (ele["networkAddresses"], ele["id"], s))
                #if ele["systemName"] == newName:
                #    return (False,"The server with IP %s and GUID %s shouldn't change it's systemName with %s!" %
                #            (ele["networkAddresses"],ele["id"],s))
        self._ensureSystemIdsDiffer(thisGUID, newId)

    def _doTest(self):
        for s in self._serverList:
            self._doSingleTest(s)

    def _doRollback(self):
        log(LOGLEVEL.INFO, "Rolling back system ids")
        for s in self._serverList:
            self._changeSystemId(s, self._oldSystemId)
        self._ensureServerSystemId(end=True)
        self._idsChanged = False

    def run(self):
        log(LOGLEVEL.INFO, "=========================================")
        log(LOGLEVEL.INFO,"LocalSystemId Test Start")
        self._ensureServerSystemId()
        ok = False

        log(LOGLEVEL.INFO, "-----------------------------------------")
        try:
            self._doTest()
            self._doRollback()
            ok = True
        except AssertionError:
            raise
        except Exception:
            log(LOGLEVEL.ERROR, "FAIL: exception occured: %s" % traceback.format_exc())
            ret = False
        finally:
            if ok:
                log(LOGLEVEL.INFO, "LocalSystemId test finished")
            if self._idsChanged:
                self._doRollback()
            log(LOGLEVEL.INFO, "=========================================")
