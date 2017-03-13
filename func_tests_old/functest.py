#!/usr/bin/env python
# -*- coding: utf-8 -*-
""" The main entry point for almost all functional tests for networkoptix mediaserver.
Contains a set of 'legacy' tests (i.e. tests being written by a previous author,
most of them are not analyzed and not refactored yet) and calls to new (or refactored)
tests which are put into sep[arate modules.
"""

import sys, time
import unittest
import urllib2
import urllib
import threading
import json
import random
import os.path
import signal
import traceback
import argparse
import subprocess
from collections import OrderedDict

from functest_util import *
from testbase import RunTests, FuncTestCase, getTestMaster, UnitTestRollback, FuncTestError
testMaster = getTestMaster()

from generator import *
import legacy_main
from rtsptests import RtspPerf, RtspTestSuit, RtspStreamTest
from sysname_test import SystemIdTest
from timetest import TimeSyncTest, TimeSyncNoInetTest, TimeSyncWithInetTest
from stortest import BackupStorageTest, MultiserverArchiveTest
from streaming_test import StreamingTest, HlsOnlyTest
from natcon_test import NatConnectionTest
from dbtest import DBTest
from cameratest import VirtualCameraTest
from instancetest import InstanceTest
from stresst import HTTPStressTest
from pycommons.Logger import initLog, log, LOGLEVEL
from pycommons.Config import config
from pycommons.FuncTest import execVBoxCmd

#class AuthH(urllib2.HTTPDigestAuthHandler):
#    def http_error_401(self, req, fp, code, msg, hdrs):
#        print "[DEBUG] Code 401"
#        print "Req: %s" % req
#        return urllib2.HTTPDigestAuthHandler.http_error_401(self, req, fp, code, msg, hdrs)


####################################################################################################

# ========================================
# Server Merge Automatic Test
# ========================================
class MergeTestBase:
    _systemName = []
    _oldSystemName = []
    _mergeTestSystemName = "mergeTest"
    _mergeTestTimeout = 1

    def __init__(self):
        self._mergeTestTimeout = testMaster.getConfig().getint("General", "mergeTestTimeout")

    # This function is used to generate unique system name but random.  It
    # will gaurantee that the generated name is UNIQUE inside of the system
    def _generateRandomSystemName(self):
        ret = []
        s = set()
        for i in xrange(len(testMaster.clusterTestServerList)):
            length = random.randint(16,30)
            while True:
                name = BasicGenerator.generateRandomString(length)
                if name in s or name == self._mergeTestSystemName:
                    continue
                else:
                    s.add(name)
                    ret.append(name)
                    break
        return ret

    # This function is used to store the old system name of each server in
    # the clusters
    def _storeClusterOldSystemName(self):
        for s in testMaster.clusterTestServerList:
            log(LOGLEVEL.DEBUG + 9, "Connection to http://%s/ec2/testConnection" % (s))
            response = urllib2.urlopen("http://%s/ec2/testConnection" % (s))
            if response.getcode() != 200:
                return False
            jobj = SafeJsonLoads(response.read(), s, 'testConnection')
            if jobj is None:
                return False
            self._oldSystemName.append(jobj["systemName"])
            response.close()
        return True

    def _setSystemName(self,addr,name):
        url = "http://%s/api/configure?%s" % (addr, urllib.urlencode({"systemName":name}))
        log(LOGLEVEL.DEBUG + 9, "Performing " + url)
        try:
            response = urllib2.urlopen(url)
            assert response.getcode() == 200, \
                "Cannot issue changeSystemName with HTTP code: %d on %s" % (response.getcode(), addr)
        except Exception as err:
            assert False, "Failed to change system name on %s: %s" % (addr, err.message)

    # This function is used to set the system name to random
    def _setClusterSystemRandom(self):
        # Store the old system name here
        self._storeClusterOldSystemName()
        testList = self._generateRandomSystemName()
        for i in xrange(len(testMaster.clusterTestServerList)):
            self._setSystemName(testMaster.clusterTestServerList[i], testList[i])

    def _setClusterToMerge(self):
        for s in testMaster.clusterTestServerList:
            self._setSystemName(s, self._mergeTestSystemName)

    def _rollbackSystemName(self):
        for i in xrange(len(testMaster.clusterTestServerList)):
            self._setSystemName(testMaster.clusterTestServerList[i], self._oldSystemName[i])


class PrepareServerStatus(BasicGenerator):
    """ Represents a single server with an UNIQUE system name.
    After we initialize this server, we will make it executes certain
    type of random data generation, after such generation, the server
    will have different states with other servers.
    Used in MergeTest_Resource.
    """
    _minData = 10
    _maxData = 20

    getterAPI = ["getResourceParams",
        "getMediaServers",
        "getMediaServersEx",
        "getCameras",
        "getUsers",
    ]
    _apiFixed = False

    _mergeTest = None

    def __init__(self,mt):
        if not self._apiFixed:
            self.getterAPI.append(testMaster.api.getServerAttr.split('/')[1])
            self.getterAPI.append(testMaster.api.getCameraAttr.split('/')[1])
            type(self)._apiFixed = True
        self._mergeTest = mt

    # Function to generate method and class matching
    def _generateDataAndAPIList(self,addr):

        return [("saveCameras", lambda num: CameraDataGenerator().generateCameraData(num, addr)),
                ("saveUser", lambda num: UserDataGenerator().generateUserData(num)),
                ("saveMediaServer", lambda num: MediaServerGenerator().generateMediaServerData(num))
                ]

    def _sendRequest(self,addr,method,d):
        url = "http://%s/ec2/%s" % (addr, method)
        try:
            sendRequest(self._mergeTest._lock, url, d, notify=True)
        except TestRequestError as err:
            return (False,"%s at %s failed with %s" % (method, addr, err.errMessage))
        return (True,"")

    def _generateRandomStates(self,addr):
        for apiName, apiGen in self._generateDataAndAPIList(addr):
            for data in apiGen(random.randint(self._minData,self._maxData)):
                ret, reason = self._sendRequest(addr, apiName, data[0])
                if ret == False:
                    return (ret, reason)
                testMaster.unittestRollback.addOperations(apiName, addr, data[1])
        return (True,"")

    def main(self,addr):
        ret,reason = self._generateRandomStates(addr)
        if ret == False:
            raise Exception("Cannot generate random states: %s" % (reason))


# This class is used to control the whole merge test
class MergeTest_Resource(MergeTestBase):
    _lock = threading.Lock()

    def _prolog(self):
        log(LOGLEVEL.INFO, "Merge test prolog : Test whether all servers you specify has the identical system name")
        oldSystemName = None
        oldSystemNameAddr = None

        # Testing whether all the cluster server has identical system name
        for s in testMaster.clusterTestServerList:
            log(LOGLEVEL.DEBUG + 9,  "Connection to http://%s/ec2/testConnection" % (s,))
            response = urllib2.urlopen("http://%s/ec2/testConnection" % (s,))
            assert response.getcode() == 200, "ec2/testConnection failed on " + s
            jobj = SafeJsonLoads(response.read(), s, 'testConnection')
            assert jobj is not None, "Bad data returned by ec2/testConnection from " + s
            if oldSystemName == None:
                oldSystemName = jobj["systemName"]
                oldSystemNameAddr = s
            else:
                systemName = jobj["systemName"]
                if systemName != oldSystemName:
                    log(LOGLEVEL.ERROR, "The merge test cannot start: different system names!")
                    log(LOGLEVEL.ERROR,"Server %s - '%s'; server %s - '%s'" % (
                        oldSystemName, oldSystemNameAddr, s, jobj["systemName"]))
                    log(LOGLEVEL.ERROR, "Please make all the server has identical system name before running merge test")
                    assert False, "Different system names before merge"
            response.close()
        log(LOGLEVEL.INFO, "Merge test prolog pass")

    def _epilog(self):
        log(LOGLEVEL.INFO, "Merge test epilog, change all servers system name back to its original one")
        self._rollbackSystemName()
        log(LOGLEVEL.INFO, "Merge test epilog done")

    # First phase will make each server has its own status
    # and also its unique system name there

    def _phase1(self):
        log(LOGLEVEL.INFO, "Merge test phase1: generate UNIQUE system name for each server and do modification")
        # 1.  Set cluster system name to random name
        self._setClusterSystemRandom()

        # 2.  Start to generate server status and data
        worker = ClusterWorker(testMaster.threadNumber, len(testMaster.clusterTestServerList))

        for s in testMaster.clusterTestServerList:
            worker.enqueue(PrepareServerStatus(self).main, (s,))

        worker.join()
        log(LOGLEVEL.INFO, "Merge test phase1 done, now sleep %s seconds and wait for sync" % self._mergeTestTimeout)
        time.sleep(self._mergeTestTimeout)

    def _phase2(self):
        log(LOGLEVEL.INFO, "Merge test phase2: set ALL the servers with system name: %s" % self._mergeTestSystemName)
        self._setClusterToMerge()
        log(LOGLEVEL.INFO, "Merge test phase2: wait %s seconds for sync" % self._mergeTestTimeout)
        # Wait until the synchronization time out expires
        time.sleep(self._mergeTestTimeout)
        # Do the status checking of _ALL_ API
        testMaster.checkMethodStatusConsistent(PrepareServerStatus.getterAPI)  # raises on errors
        log(LOGLEVEL.INFO, "Merge test phase2 done")

    def test(self):
        try:
            self._prolog()
            self._phase1()
            self._phase2()
            self._epilog()
        except AssertionError:
            raise
        except Exception as err:
            assert False, "FAIL: %s" % (err,)

# This merge test is used to test admin's password
# Steps:
# Change _EACH_ server into different system name
# Modify _EACH_ server's password into a different one
# Reconnect to _EACH_ server with _NEW_ password and change its system name back to mergeTest
# Check _EACH_ server's status that with a possible password in the list and check getMediaServer's Status
# also _ALL_ the server must be Online

# NOTES:
# I found a very radiculous truth, that if one urllib2.urlopen failed with authentication error, then the
# opener will be screwed up, and you have to reinstall that openner again. This is really stupid truth .

class MergeTest_AdminPassword(MergeTestBase):
    _newPasswordList = dict() # list contains key:serverAddr , value:password
    _oldClusterPassword = None # old cluster user password , it should be 123 always
    _username = None # User name for cluster, it should be admin
    _clusterSharedPassword = None
    _adminList = dict() # The dictionary for admin user on each server

    def __init__(self):
        # load the configuration file for username and oldClusterPassword
        config_parser = testMaster.getConfig()
        self._oldClusterPassword = config_parser.get("General","password")
        self._username = config_parser.get("General","username")

    def _generateUniquePassword(self):
        ret = []
        s = set()
        for server in testMaster.clusterTestServerList:
            # Try to generate a unique password
            pwd = BasicGenerator.generateRandomString(20)
            if pwd in s:
                continue
            else:
                # The password is new password
                ret.append((server,pwd))
                self._newPasswordList[server]=pwd

        return ret
    # This function MUST be called after you change each server's
    # password since we need to update the installer of URLLIB2

    def _setUpNewAuthentication(self, pwdlist):
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for entry in pwdlist:
            ManagerAddPassword(passman, entry[0], self._username, entry[1])
        urllib2.install_opener(urllib2.build_opener(TestDigestAuthHandler(passman)))

    def _setUpClusterAuthentication(self, password):
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for s in testMaster.clusterTestServerList:
            ManagerAddPassword(passman, s, self._username, password)
        urllib2.install_opener(urllib2.build_opener(TestDigestAuthHandler(passman)))

    def _restoreAuthentication(self):
        self._setUpClusterAuthentication(self._oldClusterPassword)

    def _merge(self):
        self._setClusterToMerge()
        time.sleep(self._mergeTestTimeout)

    def _checkPassword(self,pwd,old_server,login_server):
        log(LOGLEVEL.INFO, """Password:%s that initially modified on server:%s can log on to server:%s in new cluster
            Notes,the above server can be the same one, however since this test is after merge, it still makes sense
            Now test whether it works on the cluster""" % (pwd,old_server,login_server))
        for s in testMaster.clusterTestServerList:
            if s == login_server:
                continue
            else:
                response = None
                try:
                    response = urllib2.urlopen("http://%s/ec2/testConnection"%(s))
                except urllib2.URLError,e:
                    log(LOGLEVEL.ERROR, """This password cannot log on server:%s
                      This means this password can be used partially on cluster which is not supposed to happen
                      The cluster is not synchronized after merge
                      Error:%s""" % (s, e))
                    return False

        log(LOGLEVEL.INFO, "This password can be used on the whole cluster")
        return True

    # This function is used to probe the correct password that _CAN_ be used to log on each server
    def _probePassword(self):
        possiblePWD = None
        for entry in self._newPasswordList:
            pwd = self._newPasswordList[entry]
            # Set Up the Authencation here
            self._setUpClusterAuthentication(pwd)
            for server in testMaster.clusterTestServerList:
                response = None
                try:
                    response = urllib2.urlopen("http://%s/ec2/testConnection"%(server))
                except urllib2.URLError,e:
                    # Every failed urllib2.urlopen will screw up the opener
                    self._setUpClusterAuthentication(pwd)
                    continue # This password doesn't work

                if response.getcode() != 200:
                    response.close()
                    continue
                else:
                    possiblePWD = pwd
                    break
            if possiblePWD != None:
                if self._checkPassword(possiblePWD,entry,server):
                    self._clusterSharedPassword = possiblePWD
                    return True
                else:
                    return False
        log(LOGLEVEL.ERROR, """No password is found while probing the cluster
        This means after the merge,all the password originally on each server CANNOT be used to log on any server after merge
        This means cluster is not synchronized""")
        return False

    # This function is used to test whether all the server gets the exactly same status
    def _checkAllServerStatus(self):
        #FIXME: use exceptions instead of 'return False'
        self._setUpClusterAuthentication(self._clusterSharedPassword)
        # Now we need to set up the check
        time.sleep(testMaster.clusterTestSleepTime)
        try:
            testMaster.checkMethodStatusConsistent(["getMediaServersEx"])
        except Exception as err:
            log(LOGLEVEL.ERROR, "Fail: " + str(err))
            return False
        return True

    def _checkOnline(self,uidset,responseObj,serverAddr):
        if responseObj is None:
            return False
        for ele in responseObj:
            if ele["id"] in uidset:
                if ele["status"] != "Online":
                    # report the status
                    log(LOGLEVEL.ERROR, """Login at server:%s
                      The server:(%s) with name:%s and id:%s status is Offline
                      It should be Online after the merge
                      Status check failed""" % (serverAddr, ele["networkAddresses"],ele["name"],ele["id"]))
                    return False
        log(LOGLEVEL.INFO, "Status check for server:%s pass!" % (serverAddr))
        return True

    def _checkAllOnline(self):
        uidSet = set()
        # Set up the UID set for each registered server
        for uid in testMaster.clusterTestServerUUIDList:
            uidSet.add(uid[0])

        # For each server test whether they work or not
        for s in testMaster.clusterTestServerList:
            log(LOGLEVEL.DEBUG + 9, "Connection to http://%s/ec2/getMediaServersEx?format=json"% (s))
            response = urllib2.urlopen("http://%s/ec2/getMediaServersEx?format=json"%(s))
            if response.getcode() != 200:
                log(LOGLEVEL.ERROR, "Connection failed with HTTP code:%d"%(response.getcode()))
                return False
            if not self._checkOnline(uidSet, SafeJsonLoads(response.read(), s, 'getMediaServersEx'),s):
                return False

        return True

    def _fetchAdmin(self):
        for s in testMaster.clusterTestServerList:
            response = urllib2.urlopen("http://%s/ec2/getUsers"%(s))
            obj = SafeJsonLoads(response.read(), s, 'getUsers')
            if obj is None:
                return None
            for entry in obj:
                if entry["isAdmin"]:
                    self._adminList[s] = (entry["id"],entry["name"],entry["email"])
        return True

    def _setAdminPassword(self,ser,pwd,verbose=True):
        oldAdmin = self._adminList[ser]
        d = UserDataGenerator().createManualUpdateData(oldAdmin[0],oldAdmin[1],pwd,True,oldAdmin[2])
        req = urllib2.Request("http://%s/ec2/saveUser" % (ser), \
                data=d, headers={'Content-Type': 'application/json'})
        try:
            response =urllib2.urlopen(req)
        except:
            if verbose:
                log(LOGLEVEL.ERROR, "Connection http://%s/ec2/saveUsers failed"%(ser))
                log(LOGLEVEL.ERROR, "Cannot set admin password:%s to server:%s"%(pwd,ser))
            return False

        if response.getcode() != 200:
            response.close()
            if verbose:
                log(LOGLEVEL.ERROR, "Connection http://%s/ec2/saveUsers failed"%(ser))
                log(LOGLEVEL.ERROR, "Cannot set admin password:%s to server:%s"%(pwd,ser))
            return False
        else:
            response.close()
            return True

    # This rollback is bit of tricky since it NEEDS to rollback partial password change
    def _rollbackPartialPasswordChange(self,pwdlist):
        # Now rollback the newAuth part of the list
        for entry in pwdlist:
            if not self._setAdminPassword(entry[0],self._oldClusterPassword):
                log(LOGLEVEL.ERROR, """----------------------------------------------------------------------------------
                  +++++++++++++++++++++++++++++++++++ IMPORTANT ++++++++++++++++++++++++++++++++++++
                  Server:%s admin password cannot rollback,please set it back manually!
                  It's current password is:%s
                  It's old password is:%s
                  ----------------------------------------------------------------------------------""" % \
                    (entry[0], entry[1], self._oldClusterPassword))
        # Now set back the authentcation
        self._restoreAuthentication()

    # This function is used to change admin's password on each server
    def _changePassword(self):
        pwdlist = self._generateUniquePassword()
        uGen = UserDataGenerator()
        idx = 0
        for entry in pwdlist:
            pwd = entry[1]
            ser = entry[0]
            if self._setAdminPassword(ser,pwd):
                idx = idx+1
            else:
                # Before rollback we need to setup the authentication
                partialList = pwdlist[:idx]
                self._setUpNewAuthentication(partialList)
                # Rollback the password paritally
                self._rollbackPartialPasswordChange(partialList)
                return False
        # Set Up New Authentication
        self._setUpNewAuthentication(pwdlist)
        return True

    def _rollback(self):
        # rollback the password to the old states
        self._rollbackPartialPasswordChange(
            [(s,self._oldClusterPassword) for s in testMaster.clusterTestServerList])

        # rollback the system name
        self._rollbackSystemName()

    def _failRollbackPassword(self):
        # The current problem is that we don't know which password works so
        # we use a very conservative way to do the job. We use every password
        # that may work to change the whole cluster
        addrSet = set()

        for server in self._newPasswordList:
            pwd = self._newPasswordList[server]
            authList = [(s,pwd) for s in testMaster.clusterTestServerList]
            self._setUpNewAuthentication(authList)

            # Now try to login on to the server and then set back the admin
            check = False
            for ser in testMaster.clusterTestServerList:
                if ser in addrSet:
                    continue
                check = True
                if self._setAdminPassword(ser,self._oldClusterPassword,False):
                    addrSet.add(ser)
                else:
                    self._setUpNewAuthentication(authList)

            if not check:
                return True

        if len(addrSet) != len(testMaster.clusterTestServerList):
            log(LOGLEVEL.ERROR,"""There're some server's admin password I cannot prob and rollback
                Since it is a failover rollback,I cannot guarantee that I can rollback the whole cluster
                There're possible bugs in the cluster that make the automatic rollback impossible
                The following server has _UNKNOWN_ password now""")
            for ser in testMaster.clusterTestServerList:
                if ser not in addrSet:
                    log(LOGLEVEL.ERROR, "The server:%s has _UNKNOWN_ password for admin" % (ser))
            return False
        else:
            return True

    def _failRollback(self):
        log(LOGLEVEL.INFO,"""===========================================
            Start Failover Rollback
            This rollback will _ONLY_ happen when the merge test failed
            This rollback cannot guarantee that it will rollback everything
            Detail information will be reported during the rollback""")
        if self._failRollbackPassword():
            self._restoreAuthentication()
            self._rollbackSystemName()
            log(LOGLEVEL.INFO, "Failover Rollback Done!")
        else:
            log(LOGLEVEL.ERROR, "Failover Rollback Failed!")
        log(LOGLEVEL.INFO, "===========================================")

    def test(self):
        log(LOGLEVEL.INFO, "===========================================")
        log(LOGLEVEL.INFO, "Merge Test:Admin Password Test Start!")
        # At first, we fetch each system's admin information
        if self._fetchAdmin() is None:
            log(LOGLEVEL.ERROR, "Merge Test:Fetch Admins list failed")
            return False
        # Change each system into different system name
        log(LOGLEVEL.INFO, "Now set each server node into different and UNIQUE system name\n")
        self._setClusterSystemRandom()
        # Change the password of _EACH_ servers
        log(LOGLEVEL.INFO, "Now change each server node's admin password to a UNIQUE password\n")
        if not self._changePassword():
            log(LOGLEVEL.ERROR, "Merge Test:Admin Password Test Failed")
            return False
        log(LOGLEVEL.INFO, "Now set the system name back to mergeTest and wait for the merge\n")
        self._merge()
        # Now start to probing the password
        log(LOGLEVEL.INFO, "Start to prob one of the possible password that can be used to LOG to the cluster\n")
        if not self._probePassword():
            log(LOGLEVEL.ERROR, "Merge Test:Admin Password Test Failed")
            self._failRollback()
            return False
        log(LOGLEVEL.INFO, "Check all the server status\n")
        # Now start to check the status
        if not self._checkAllServerStatus():
            log(LOGLEVEL.ERROR, "Merge Test:Admin Password Test Failed")
            self._failRollback()
            return False
        log(LOGLEVEL.INFO, "Check all server is Online or not")
        if not self._checkAllOnline():
            log(LOGLEVEL.ERROR, "Merge Test:Admin Password Test Failed")
            self._failRollback()
            return False

        log(LOGLEVEL.INFO, "Lastly we do rollback\n")
        self._rollback()

        log(LOGLEVEL.INFO, "Merge Test:Admin Password Test Pass!")
        log(LOGLEVEL.INFO, "===========================================")
        return True


def MergeTestRun(needCleanUp=False):
    cleanUp = needCleanUp
    ok = False
    try:
        log(LOGLEVEL.INFO, "================================")
        print "Server Merge Test: Resource Start\n"
        MergeTest_Resource().test()
        # The following merge test ALWAYS fail and I don't know it is my problem or not
        # Current it is disabled and you could use a seperate command line to run it
        #MergeTest_AdminPassword().test()
        ok = True
        return True
    except AssertionError as err:
        log(LOGLEVEL.ERROR, "FAIL: " + err.message)
        return False
    finally:
        log(LOGLEVEL.INFO, "Server Merge Test: Resource End%s" % ('' if ok else ": test FAILED"))
        log(LOGLEVEL.INFO, "================================\n")
        if cleanUp:
            doCleanUp()



# ===================================
# Performance test function
# only support add/remove ,value can only be user and media server
# ===================================

class PerformanceOperation():
    _lock = threading.Lock()

    def _filterOutId(self,list):
        ret = []
        for i in list:
            ret.append(i[0])
        return ret

    def _sendRequest(self,methodName,d,server):
        url = "http://%s/ec2/%s" % (server,methodName)
        try:
            sendRequest(self._lock, url, d)
        except TestRequestError as err:
            log(LOGLEVEL.ERROR, "%s failed: %s" % (methodName, err.message))
        else:
            log(LOGLEVEL.DEBUG + 9, "%s OK" % (methodName))

    def _getUUIDList(self,methodName):
        ret = []
        for s in testMaster.clusterTestServerList:
            data = []

            response = urllib2.urlopen("http://%s/ec2/%s?format=json" % (s,methodName))

            if response.getcode() != 200:
                return None

            json_obj = SafeJsonLoads(response.read(), s, methodName)
            if json_obj is None:
                return None
            for entry in json_obj:
                if "isAdmin" in entry and entry["isAdmin"] == True:
                    continue # Skip the admin
                data.append(entry["id"])

            response.close()

            ret.append((s,data))

        return ret

    # This function will retrieve data that has name prefixed with ec2_test as
    # prefix
    # This sort of resources are generated by our software .

    def _getFakeUUIDList(self,methodName):
        ret = []
        for s in testMaster.clusterTestServerList:
            data = []

            response = urllib2.urlopen("http://%s/ec2/%s?format=json" % (s,methodName))

            if response.getcode() != 200:
                return None

            json_obj = SafeJsonLoads(response.read(), s, methodName)
            if json_obj is None:
                return None
            for entry in json_obj:
                if "name" in entry and entry["name"].startswith("ec2_test"):
                    if "isAdmin" in entry and entry["isAdmin"] == True:
                        continue
                    data.append(entry["id"])
            response.close()
            ret.append((s,data))
        return ret

    def _sendOp(self,methodName,dataList,addr):
        worker = ClusterWorker(testMaster.threadNumber, len(dataList))
        for d in dataList:
            worker.enqueue(self._sendRequest,(methodName,d,addr))
        worker.join()

    _resourceRemoveTemplate = """
        {
            "id":"%s"
        }
    """
    def _removeAll(self,uuidList):
        data = []
        for entry in uuidList:
            for uuid in entry[1]:
                data.append(self._resourceRemoveTemplate % (uuid))
            self._sendOp("removeResource",data,entry[0])

    def _remove(self,uuid):
        self._removeAll([("127.0.0.1:7001",[uuid])])


class UserOperation(PerformanceOperation):
    def add(self,num):
        gen = UserDataGenerator()
        for s in testMaster.clusterTestServerList:
            self._sendOp("saveUser",
                         self._filterOutId(gen.generateUserData(num)),s)

        return True

    def remove(self,who):
        self._remove(who)
        return True

    def removeAll(self):
        uuidList = self._getUUIDList("getUsers?format=json")
        if uuidList == None:
            return False
        self._removeAll(uuidList)
        return True

    def removeAllFake(self):
        uuidList = self._getFakeUUIDList("getUsers?format=json")
        if uuidList == None:
            return False
        self._removeAll(uuidList)
        return True


class MediaServerOperation(PerformanceOperation):
    def add(self,num):
        gen = MediaServerGenerator()
        for s in testMaster.clusterTestServerList:
            self._sendOp("saveMediaServer",
                         self._filterOutId(gen.generateMediaServerData(num)),s)
        return True

    def remove(self, uuid):
        self._remove(uuid)
        return True

    def removeAll(self):
        uuidList = self._getUUIDList("getMediaServersEx?format=json")
        if uuidList == None:
            return False
        self._removeAll(uuidList)
        return True

    def removeAllFake(self):
        uuidList = self._getFakeUUIDList("getMediaServersEx?format=json")
        if uuidList == None:
            return False
        self._removeAll(uuidList)
        return True


class CameraOperation(PerformanceOperation):
    def add(self,num):
        gen = CameraDataGenerator()
        for s in testMaster.clusterTestServerList:
            self._sendOp("saveCameras",
                         self._filterOutId(gen.generateCameraData(num,s)),s)
        return True

    def remove(self,uuid):
        self._remove(uuid)
        return True

    def removeAll(self):
        uuidList = self._getUUIDList("getCameras?format=json")
        if uuidList == None:
            return False
        self._removeAll(uuidList)
        return True

    def removeAllFake(self):
        uuidList = self._getFakeUUIDList("getCameras?format=json")
        if uuidList == None:
            return False
        self._removeAll(uuidList)
        return True


def doClearAll(fake=False):
    if fake:
        CameraOperation().removeAllFake()
        UserOperation().removeAllFake()
        MediaServerOperation().removeAllFake()
    else:
        CameraOperation().removeAll()
        UserOperation().removeAll()
        MediaServerOperation().removeAll()


def runMiscFunction(argc, argv):
    log(LOGLEVEL.INFO, "runMiscFunction(%s, %s)" % (argc, argv))
    if argc not in (1, 2):
        return (False,"2/1 parameters are needed")

    l = argv[0].split('=')

    if l[0] != '--add' and l[0] != '--remove':
        return (False,"Unknown first parameter option %s" % (l[0],))

    t = globals()["%sOperation" % (l[1])]

    if t == None:
        return (False,"Unknown target operations: %s" % (l[1]))
    else:
        t = t()

    if l[0] == '--add':
        if argc != 2:
            return (False,"--add must have --count option")
        l = argv[1].split('=')
        if l[0] == '--count':
            num = int(l[1])
            if num <= 0 :
                return (False,"--count must be positive integer")
            if t.add(num) == False:
                return (False,"cannot perform add operation")
        else:
            return (False,"--add can only have --count options")
    elif l[0] == '--remove':
        if argc == 2:
            l = argv[1].split('=')
            if l[0] == '--id':
                if t.remove(l[1]) == False:
                    return (False,"cannot perform remove UID operation")
            elif l[0] == '--fake':
                if t.removeAllFake() == False:
                    return (False,"cannot perform remove UID operation")
            else:
                return (False,"--remove can only have --id options")
        elif argc == 1:
            if t.removeAll() == False:
                return (False,"cannot perform remove all operation")
    else:
        return (False,"Unknown command:%s" % (l[0]))

    return True


# ===================================
# Perf Test
# ===================================
class SingleResourcePerfGenerator(object):
    def generateUpdate(self,id,parentId):
        pass

    def generateCreation(self,parentId):
        pass

    def saveAPI(self):
        pass

    def updateAPI(self):
        pass

    def getAPI(self):
        pass

    def resourceName(self):
        pass

class SingleResourcePerfTest:
    _creationProb = 0.333
    _updateProb = 0.5
    _resourceList = []
    _lock = threading.Lock()
    _globalLock = None
    _resourceGen = None
    _serverAddr = None
    _initialData = 10
    _deletionTemplate = """
        {
            "id":"%s"
        }
    """

    def __init__(self,glock,gen,addr):
        self._globalLock = glock
        self._resourceGen = gen
        self._serverAddr = addr
        self._initializeResourceList()

    def _initializeResourceList(self):
        for _ in xrange(self._initialData):
            self._create()

    def _create(self):
        d = self._resourceGen.generateCreation(self._serverAddr)
        # Create the resource in the remote server
        response = None

        with self._globalLock:
            req = urllib2.Request("http://%s/ec2/%s" % (self._serverAddr,self._resourceGen.saveAPI()),
                data=d[0], headers={'Content-Type': 'application/json'})
            try:
                response = urllib2.urlopen(req)
            except urllib2.URLError,e:
                return False

        if response.getcode() != 200:
            response.close()
            return False
        else:
            response.close()
            testMaster.unittestRollback.addOperations("saveCameras", self._serverAddr, d[1])
            with self._lock:
                self._resourceList.append(d[1])
                return True

    def _remove(self):
        id = None
        # Pick up a deleted resource Id
        with self._lock:
            if len(self._resourceList) == 0:
                return True
            idx = random.randint(0,len(self._resourceList) - 1)
            id = self._resourceList[idx]
            # Do the deletion from the list FIRST
            del self._resourceList[idx]

        # Do the deletion on remote machine
        with self._globalLock:
            req = urllib2.Request("http://%s/ec2/removeResource" % (self._serverAddr),
                data=self._deletionTemplate % (id), headers={'Content-Type': 'application/json'})
            try:
                response = urllib2.urlopen(req)
            except urllib2.URLError,e:
                return False

            if response.getcode() != 200:
                response.close()
                return False
            else:
                return True

    def _update(self):
        id = None
        with self._lock:
            if len(self._resourceList) == 0:
                return True
            idx = random.randint(0,len(self._resourceList) - 1)
            id = self._resourceList[idx]
            # Do the deletion here in order to ensure that another thread will NOT
            # delete it
            del self._resourceList[idx]

        # Do the updating on the remote machine here
        d = self._resourceGen.generateUpdate(id,self._serverAddr)

        with self._globalLock:
            req = urllib2.Request("http://%s/ec2/%s" % (self._serverAddr,self._resourceGen.updateAPI()),
                data=d, headers={'Content-Type': 'application/json'})
            try:
                response = urllib2.urlopen(req)
            except urllib2.URLError,e:
                return False
            if response.getcode() != 200:
                response.close()
                return False
            else:
                return True

        #XXX not used!
        # Insert that resource _BACK_ to the list
        with self._lock:
            self._resourceList.append(id)

    def _takePlace(self,prob):
        if random.random() <= prob:
            return True
        else:
            return False

    def runOnce(self):
        if self._takePlace(self._creationProb):
            return (self._create(),"Create")
        elif self._takePlace(self._updateProb):
            return (self._update(),"Update")
        else:
            return (self._remove(),"Remove")

class CameraPerfResourceGen(SingleResourcePerfGenerator):
    _gen = None

    def __init__(self):
        if self._gen is None:
            type(self)._gen = CameraDataGenerator()

    def generateUpdate(self,id,parentId):
        return self._gen.generateUpdateData(id,parentId)[0]

    def generateCreation(self,parentId):
        return self._gen.generateCameraData(1,parentId)[0]

    def saveAPI(self):
        return "saveCameras"

    def updateAPI(self):
        return "saveCameras"

    def getAPI(self):
        return "getCameras"

    def resourceName(self):
        return "Camera"

class UserPerfResourceGen(SingleResourcePerfGenerator):
    _gen = None

    def __init__(self):
        if self._gen is None:
            type(self)._gen = UserDataGenerator()

    def generateUpdate(self,id,parentId):
        return self._gen.generateUpdateData(id)[0]

    def generateCreation(self,parentId):
        return self._gen.generateUserData(1)[0]

    def saveAPI(self):
        return "saveUser"

    def getAPI(self):
        return "getUsers"

    def resourceName(self):
        return "User"

class PerfStatistic:
    createOK = 0
    createFail=0
    updateOK =0
    updateFail=0
    removeOK = 0
    removeFail=0

class PerfTest:
    _globalLock = threading.Lock()
    _statics = dict()
    _perfList = []
    _exit = False
    _threadPool = []

    def _onInterrupt(self,a,b):
        self._exit = True

    def _initPerfList(self,type):
        for s in testMaster.clusterTestServerList:
            if type[0]:
                self._perfList.append((s,SingleResourcePerfTest(self._globalLock,UserPerfResourceGen(),s)))

            if type[1]:
                self._perfList.append((s,SingleResourcePerfTest(self._globalLock,CameraPerfResourceGen(),s)))

            self._statics[s] = PerfStatistic()

    def _threadMain(self):
        while not self._exit:
            for entry in self._perfList:
                serverAddr = entry[0]
                object = entry[1]
                ret,type = object.runOnce()
                if type == "Create":
                    if ret:
                        self._statics[serverAddr].createOK = self._statics[serverAddr].createOK + 1
                    else:
                        self._statics[serverAddr].createFail= self._statics[serverAddr].createFail + 1
                elif type == "Update":
                    if ret:
                        self._statics[serverAddr].updateOK = self._statics[serverAddr].updateOK + 1
                    else:
                        self._statics[serverAddr].updateFail = self._statics[serverAddr].updateFail + 1
                else:
                    if ret:
                        self._statics[serverAddr].removeOK = self._statics[serverAddr].removeOK + 1
                    else:
                        self._statics[serverAddr].removeFail = self._statics[serverAddr].removeFail + 1

    def _initThreadPool(self,threadNumber):
        for _ in xrange(threadNumber):
            th = threading.Thread(target=self._threadMain)
            th.start()
            self._threadPool.append(th)

    def _joinThreadPool(self):
        for th in self._threadPool:
            th.join()

    def _parseParameters(self,par):
        sl = par.split(',')
        ret = [False,False,False]
        for entry in sl:
            if entry == 'Camera':
                ret[1] = True
            elif entry == 'User':
                ret[0] = True
            elif entry == 'MediaServer':
                ret[2] = True
            else:
                continue
        return ret

    def run(self, par):
        ret = self._parseParameters(par)
        if not ret[0] and not ret[1] and not ret[2]:
            return False
        # initialize the performance test list object
        log(LOGLEVEL.INFO, "Start to prepare performance data")
        log(LOGLEVEL.INFO, "Please wait patiently")
        self._initPerfList(ret)
        # start the thread pool to run the performance test
        log(LOGLEVEL.INFO, "======================================")
        log(LOGLEVEL.INFO, "Performance Test Start")
        log(LOGLEVEL.INFO, "Hit CTRL+C to interrupt it")
        self._initThreadPool(testMaster.threadNumber)
        # Waiting for the user to stop us
        signal.signal(signal.SIGINT,self._onInterrupt)
        while not self._exit:
            try:
                time.sleep(0)
            except:
                break
        # Join the thread now
        self._joinThreadPool()
        # Print statistics now
        log(LOGLEVEL.INFO, "===================================")
        log(LOGLEVEL.INFO, "Performance Test Done")
        for key,value in self._statics.iteritems():
            log(LOGLEVEL.INFO,"---------------------------------")
            log(LOGLEVEL.INFO, "Server:%s" % (key))
            log(LOGLEVEL.INFO, "Resource Create Success: %d" % (value.createOK))
            log(LOGLEVEL.INFO, "Resource Create Fail:    %d" % (value.createFail))
            log(LOGLEVEL.INFO, "Resource Update Success: %d" % (value.updateOK))
            log(LOGLEVEL.INFO, "Resource Update Fail:    %d" % (value.updateFail))
            log(LOGLEVEL.INFO, "Resource Remove Success: %d" % (value.removeOK))
            log(LOGLEVEL.INFO, "Resource Remove Fail:    %d" % (value.removeFail))
            log(LOGLEVEL.INFO, "---------------------------------")
        log(LOGLEVEL.INFO, "====================================")
        return True


def runPerfTest(argv):
    l = argv[2].split('=')
    rc = PerfTest().run(l[1])
    doCleanUp()
    return rc


def doCleanUp(reinit=False):
    selection = '' if testMaster.args.autorollback else 'x'
    if not testMaster.args.autorollback:
        try :
            selection = raw_input("Press Enter to continue ROLLBACK or press x to SKIP it...")
        except:
            pass

    if not selection.startswith('x'):
        log(LOGLEVEL.INFO, "Starting rollback, do not interrupt!")
        testMaster.unittestRollback.doRollback()
        log(LOGLEVEL.INFO, "ROLLBACK DONE")
    else:
        log(LOGLEVEL.INFO, "Rollback skipped, you could use --recover to perform rollback later")
    if reinit:
        testMaster.init_rollback()


#TODO: make use of it!
def print_tests(suit, shift='    '):
    for test in suit:
        if isinstance(test, unittest.TestSuite):
            log(LOGLEVEL.DEBUG + 9, "DEBUG:%s[%s]:" % (shift, type(test)))
            print_tests(test, shift+'    ')
        else:
            log(LOGLEVEL.DEBUG + 9, "DEBUG:%s%s" % (shift, test))


def CallTest(testClass):
    ###if not testMaster.openerReady:
    ###    testMaster.setUpPassword()
    # this print is used by FunctestParser.parse_timesync_start
    log(LOGLEVEL.INFO, "%s suites: %s" % (testClass.__name__, ', '.join(testClass.iter_suites())))
    return RunTests(testClass)


class MainFunctests(FuncTestCase):
    """
    Provides an object to use virtual box control methods
    """
    helpStr = "The main minimal functional tests set"
    _test_name = "Main functests"
    _test_key = "legacy"
    _suites = (("Main", [
        "ConnectionTest",
        "InitialClusterTest",
        "BasicClusterTest",
        "MergeTest",
        "SysnameTest",
    ]),)
    _need_rollback = True
    _basicTestOk = False

    @classmethod
    def globalInit(cls, config):
        super(MainFunctests, cls).globalInit(config)
        testMaster.args.autorollback = True

    #@classmethod
    #def tearDownClass(cls):
    #    if cls._need_rollback and testMaster.unittestRollback:
    #        doCleanUp()
    #    super(MainFunctests, cls).tearDownClass()

    ########

    def ConnectionTest(self):
        self._servers_th_ctl('safe-start')
        self._wait_servers_up()
        self.assertTrue(testMaster.testConnection(frame=False), "Connection Test failed")

    def _checkSkipLegacy(self):
        if testMaster.args.skiplegacy:
            self.skipTest("Disabled by --skiplegacy option")

    def InitialClusterTest(self):
        self._checkSkipLegacy()
        testMaster.initial_tests()
        # https://networkoptix.atlassian.net/browse/VMS-4599
        for host in self.hosts:
            try:
                execVBoxCmd(host, '[ -d "/boot/HD Witness Media" ]')
                raise Exception(
                    "'%s': create unnecessary '/boot/HD Witness Media' folder" % host)
            except subprocess.CalledProcessError:
                pass

    def BasicClusterTest(self):
        self._checkSkipLegacy()
        try:
            the_test = unittest.main(module=legacy_main, exit=False, argv=[sys.argv[0]],
                                     testRunner=unittest.TextTestRunner(
                                         stream=sys.stdout,
                                     ))
            assert the_test.result.wasSuccessful(), "Basic functional test FAILED"
            type(self)._basicTestOk = True
        finally:
            if testMaster.unittestRollback:
                doCleanUp()

    def _skipIfBasicFailed(self):
        if not self._basicTestOk:
            self.skipTest("Basic tests failed")

    def MergeTest(self):
        self._checkSkipLegacy()
        self._skipIfBasicFailed()
        if testMaster.unittestRollback:
            testMaster.init_rollback()
        try:
            MergeTest_Resource().test()
        finally:
            if testMaster.unittestRollback:
                doCleanUp()

    def SysnameTest(self):
        self._checkSkipLegacy()
        self._skipIfBasicFailed()
        SystemIdTest(testMaster.getConfig()).run()
        testMaster.checkServerListStates()

def RunByAutotest():
    """
    Used when this script is called by the autotesting script auto.py
    """
    testMaster.init(notest=True)
    CallTest(MainFunctests)
    if not testMaster.args.mainonly:
        if not testMaster.args.skiptime:
            CallTest(TimeSyncTest)
        if not testMaster.args.skipbak:
            CallTest(BackupStorageTest)
        if not testMaster.args.skipmsa:
            CallTest(MultiserverArchiveTest)
        if not testMaster.args.skipstrm:
            CallTest(StreamingTest)
        if not testMaster.args.skipdbup:
            CallTest(DBTest)
        if not testMaster.args.skipinstance:
          CallTest(InstanceTest)
        if not testMaster.args.skipcamera:
          CallTest(VirtualCameraTest)
        CallTest(HTTPStressTest)
    #FIXME: acureate test result processing required!!!
    log(LOGLEVEL.INFO, "\nALL AUTOMATIC TEST ARE DONE\n")
    return True


# These are the old legasy tests, just organized a bit
SimpleTestKeys = {
    '--sys-id': SystemIdTest,
    '--sysid': SystemIdTest,
    '--rtsp-test': RtspTestSuit,
    '--rtsp-perf': RtspPerf,
    '--rtsp-stream': RtspStreamTest,
}

# Tests to be run on the vargant boxes, separately or within the autotest sequence
BoxTestKeys = OrderedDict([
    ('--timesync', TimeSyncTest),
    ('--ts-noinet', TimeSyncNoInetTest),
    ('--ts-inet', TimeSyncWithInetTest),
    ('--bstorage', BackupStorageTest),
    ('--msarch', MultiserverArchiveTest),
    ('--stream', StreamingTest),
    ('--hlso', HlsOnlyTest),
    ('--dbup', DBTest),
    ('--camera', VirtualCameraTest),
    ('--htstress', HTTPStressTest),
    ('--natcon', NatConnectionTest),
    ('--instance', InstanceTest),
    ('--boxtests', None),
])
KeysSkipList = ('--boxtests', '--ts-noinet', '--ts-inet', '--hlso')


def BoxTestsRun(name):
    testMaster.init(notest=True)
    if name == 'boxtests':
        ok = True
        if not CallTest(MainFunctests): ok = False
        if not CallTest(TimeSyncTest): ok = False
        if not CallTest(BackupStorageTest): ok = False
        if not CallTest(MultiserverArchiveTest): ok = False
        if not CallTest(StreamingTest): ok = False
        if not CallTest(DBTest): ok = False
        if not CallTest(InstanceTest): ok = False
        if not CallTest(VirtualCameraTest): ok = False
        if not CallTest(HTTPStressTest): ok = False
        return ok
    else:
        return CallTest(BoxTestKeys['--' + name])


def LegacyTestsRun(only = False, argv=[]):
    _argv = argv[:]
    _argv.insert(0, sys.argv[0])
    del _argv[1:(3 if only else 2)]
    with testMaster.unittestRollback:
        the_test = unittest.main(module=legacy_main, exit=False, argv=_argv)
    #doCleanUp(reinit=True)

    if the_test.result.wasSuccessful():
        log(LOGLEVEL.INFO, "Main tests passed OK")
        if (not only):
            with testMaster.unittestRollback:
                mergeOk = MergeTestRun()
            if mergeOk:
                with testMaster.unittestRollback:
                    try:
                        SystemIdTest(testMaster.getConfig()).run()
                    except AssertionError as err:
                        log(LOGLEVEL.ERROR, "SystemNIdTest FAILED: " + err.message)
    #doCleanUp()


def DoTests(argv):
    log(LOGLEVEL.INFO, "The automatic test starts, please wait for checking cluster status, test connection and APIs and do proper rollback...")
    # initialize cluster test environment
    argc = len(argv)
    testMaster.init()

    if argc == 1 and argv[0] in SimpleTestKeys:
        try:
            return SimpleTestKeys[argv[0]](testMaster.getConfig()).run()
        except AssertionError as err:
            log(LOGLEVEL.ERROR, "%s FAILED: %s" % (argv[0], err.message))
            return False

    try:
        testMaster.initial_tests()
    except AssertionError, err:
        print "FAIL: initial cluster test: " + err.message
        return False

    if argc == 1 and argv[0] == '--sync':
        return True # done here, since we just need to test whether
               # all the servers are on the same page

    elif argc >= 1 and argv[0] == '--legacy':
        LegacyTestsRun(argv[1] == '--only' if argc >= 2 else False, argv)
        #FIXME no result code returning!

    elif argc == 1 and argv[0] == '--main':
        rc = LegacyTestsRun()

        log(LOGLEVEL.INFO, "\nALL AUTOMATIC TEST ARE DONE\n")
        #FIXME no result code returning!

    elif (argc == 1 or argc == 2) and argv[0] == '--clear':
        if argc == 2:
            if argv[1] == '--fake':
                doClearAll(True)
            else:
                print "Unknown option: %s in --clear" % (argv[1])
        else:
            doClearAll(False)
        testMaster.unittestRollback.removeRollbackDB()
        #FIXME no result code returning!

    elif argc == 1 and argv[0] == '--perf':
        PerfTest().start()
        doCleanUp()
        #FIXME no result code returning!

    else:
        if argv[0] == '--merge-test':
            return MergeTestRun(needCleanUp=True)
        elif argv[0] == '--merge-admin':
            rc = MergeTest_AdminPassword().test()
            testMaster.unittestRollback.removeRollbackDB()
            return rc
        elif argc == 2 and argv[0] == '--perf':
            return runPerfTest()
        else:
            res = runMiscFunction(argc, argv)
            if not res[0]:
                log(LOGLEVEL.ERROR, "ERROR: " + res[1])
            return res[0]
        #FIXME no result code returning!


class BoxTestAction(argparse.Action):
    def __init__(self, option_strings, dest, help=None):
        super(BoxTestAction, self).__init__(
            option_strings=option_strings, nargs=0, dest=dest, help=help)

    def __call__(self, parser, namespace, values, option_string=None):
        namespace.BoxTest = self.dest


CommonArgs = ('config', 'autorollback', 'log')


def parseArgs():
    parser = argparse.ArgumentParser()

    parser.add_argument('--help-arg', metavar="ARG", nargs="?", const="", help="Additional help for some (legacy) options. Without argument print the list of that options.")
    parser.add_argument('-c', '--config', metavar="FILE", help="Use alternative configuration file")
    parser.add_argument('--recover', action="store_true", help=getHelpDesc('recover'))
    parser.add_argument('--log', metavar="FILE", nargs="?", const="", help="Suppress direct output, storing it into a file. See '--help-arg log' for details ")
    parser.add_argument('--loglevel', type=int, default=15, help="Log level for the functest logger" )

    parser.add_argument('--list-auto-test', action="store_true", help="List options for tests, used in auto-testing system" )

    parser.add_argument('--autorollback', '--arb', action="store_true", help="Automativally rollback changes done by the legacy tests")
    parser.add_argument('--skiplegacy', action="store_true", help="Skip 'legacy' functional tests")
    parser.add_argument('--skiptime', action="store_true", help="Skip time synchronization tests")
    parser.add_argument('--skipbak', action="store_true", help="Skip backup storage tests")
    parser.add_argument('--skipmsa', action="store_true", help="Skip multi-server archive tests")
    parser.add_argument('--skipstrm', action="store_true", help="Skip streaming tests")
    parser.add_argument('--skipdbup', action="store_true", help="Skip DB upgrae test")
    parser.add_argument('--skipcamera', action="store_true", help="Skip virtual camera test")
    parser.add_argument('--skipinstance', action="store_true", help="Skip instance test")
    parser.add_argument('--dump', action="store_true", help="Create dump files during RTSP perf tests")


    group = parser.add_argument_group("Functional test selection").add_mutually_exclusive_group()
    boxKey = None
    for key, klass in BoxTestKeys.iteritems():
        if klass is None:
            if boxKey is not None:
                raise Exception("Two or more options in BoxTestKeys have 'None' value!")
            boxKey = key
        else:
            group.add_argument(key, action=BoxTestAction, help="Run only: " + klass.helpStr)
    else:
        group.add_argument(boxKey, action=BoxTestAction, help="Run all vm-using functional tests")

    #TODO: if I do two steps of argparsing I should think about --help to pring the second step args too!!!

    #parser.add_argument()

    args, other = parser.parse_known_args()
    #args.natcon = '--natcon' in other # we need it as a flag
    #if args.log is not None and getattr(args, 'BoxTest', None) is None:
    #    print "WARNING: --log is used only with one of 'Functional test selection' arguments!"
    return args, other


def ListAutoTests():
    for key, klass in BoxTestKeys.iteritems():
        if key not in KeysSkipList:
            print "%s %s" % (key, klass.helpStr)


def main(args, other):
    #print "Args: %s" % (args,)
    #print "Remaining argv: %s" % (other,)
    config.read(args.config)
    if args.help_arg is not None:
        showHelp(args.help_arg)
        return True
    if args.list_auto_test:
        ListAutoTests()
        return True
    initLog(args.loglevel, args.log, rewrite=True)
    if args.recover:
        UnitTestRollback(autorollback=True, nocreate=True)
        return True
    testMaster.applyArgs(args)
    try:
        if other:
            return DoTests(other)
        elif getattr(args, 'BoxTest', False):
            # box-tests can run without complete testMaster.init(), since they reinitialize mediaserver
            return BoxTestsRun(args.BoxTest)
        else: # called from auto.py, using boxes which are created, but servers not started
            return RunByAutotest()
    except FuncTestError as err:
        print "FAIL: functional test failed: %s, %s" % err.args
        return False
    finally:
        if testMaster.log:
            testMaster.log.unbind()

if __name__ == '__main__':
    reload(sys)
    sys.setdefaultencoding('utf8')
    if not main(*parseArgs()):
        sys.exit(1)
