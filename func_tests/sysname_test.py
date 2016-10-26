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

from functest_util import SafeJsonLoads
from generator import BasicGenerator

class SystemNameTest:
    _oldSystemName = None
    _syncTime = 2

    def __init__(self, config):
        self._namesUsed = set()
        self._guidDict = dict()
        self._serverList = []
        self._config = config
        self._serverList = self._config.rtget("ServerList")
        self._syncTime = self._config.rtget("SleepTime")

    def _doGet(self,addr,methodName):
        print "Connection to http://%s/ec2/%s" % (addr,methodName)
        try:
            response = urllib2.urlopen("http://%s/ec2/%s" % (addr,methodName))
            return response.read() if response.getcode() == 200 else None
        except Exception as err:
            print "Exception: %s, %s" % sys.exc_info()[0:2]
            return None

    def _changeSystemName(self,addr,name):
        url = "http://%s/api/configure?%s" % (addr,urllib.urlencode({"systemName":name}))
        print "Request:", url
        response = urllib2.urlopen(url)
        return response.getcode() == 200

    def _ensureServerSystemName(self):
        systemName = None

        for s in self._serverList:
            obj = self._config.rtget("ServerObjs")[s]
            if systemName != None:
                if systemName != obj["systemName"]:
                    return (False,"Server: %s has systemName: %s which is different with others: %s" % (s,obj["systemName"],systemName))
            else:
                systemName = obj["systemName"]

            self._guidDict[s] = obj["ecsGuid"]
        self._oldSystemName = systemName
        return (True,"")

    def _doSingleTest(self,s):
        thisGUID = self._guidDict[s]
        while True: # ensure the name is unique
            newName = BasicGenerator.generateRandomString(20)
            if newName not in self._namesUsed:
                self._namesUsed.add(newName)
                break
        self._changeSystemName(s, newName)
        # wait for the time to sync
        time.sleep(self._syncTime)
        # issue a getMediaServerEx request to test whether all the servers in
        # the list has the expected offline/online status
        ret = self._doGet(s, "getMediaServersEx")
        if ret is None:
            return (False,"doGet failed on server %s with getMediaServersEx" % (s))
        obj = SafeJsonLoads(ret, s, "getMediaServersEx")
        if obj is None:
            return (False, "The server %s sends wrong response to getMediaServersEx" % (s,))
        # After changing system name the server shouldn't forget of the another, but should think
        # the another server is offline (since they aren't in the same system).
        for ele in obj:
            if ele["id"] == thisGUID:
                if ele["status"] == "Offline":
                    return (False,"The server %s with GUID %s should be Online" % (s,thisGUID))
                if ele["systemName"] != newName:
                    return (False,"The server %s with GUID %s hasn't changed its systemName!" % (s,thisGUID))
            else:
                if ele["status"] == "Online":
                    return (False,"The server with IP %s and GUID %s should be Offline when login on Server: %s" %
                            (ele["networkAddresses"],ele["id"],s))
                if ele["systemName"] == newName:
                    return (False,"The server with IP %s and GUID %s shouldn't change it's systemName with %s!" %
                            (ele["networkAddresses"],ele["id"],s))
        return (True,"")

    def _doTest(self):
        for s in self._serverList:
            ret, reason = self._doSingleTest(s)
            if not ret:
                return (ret,reason)
        return (True,"")

    def _doRollback(self):
        print "Rolling back system names"
        for s in self._serverList:
            self._changeSystemName(s,self._oldSystemName)

    def run(self):
        print "========================================="
        print "SystemName Test Start"
        ret,reason = self._ensureServerSystemName()

        if ret:
            print "-----------------------------------------"
            print "Start to test SystemName for server lists"
            try:
                ret,reason = self._doTest()
                if not ret:
                    print "FAIL: %s" % reason
            except Exception:
                print "FAIL: exception occured: %s" % traceback.format_exc()
                ret = False

            print "SystemName test finished"
            self._doRollback()
        else:
            print "FAIL: %s" % reason

        print "========================================="
        return ret
