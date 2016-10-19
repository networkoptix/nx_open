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

from functest_util import *
from testbase import RunTests as RunBoxTests, LegacyTestWrapper, getTestMaster, UnitTestRollback
testMaster = getTestMaster()

from generator import *
import legacy_main
from rtsptests import RtspPerf, RtspTestSuit, RtspStreamTest
from sysname_test import SystemNameTest
from timetest import TimeSyncTest, TimeSyncNoInetTest, TimeSyncWithInetTest
from stortest import BackupStorageTest, MultiserverArchiveTest
from streaming_test import StreamingTest, HlsOnlyTest
from natcon_test import NatConnectionTest
from dbtest import DBTest
from proxytest import ProxyTest


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
            print "Connection to http://%s/ec2/testConnection" % (s)
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
        print "Connection to http://%s/api/configure" % (addr)
        response = urllib2.urlopen("http://%s/api/configure?%s" % (addr,urllib.urlencode({"systemName":name})))
        if response.getcode() != 200 :
            return (False,"Cannot issue changeSystemName with HTTP code:%d to server:%s" % (response.getcode()),addr)
        response.close()
        return (True,"")

    # This function is used to set the system name to random
    def _setClusterSystemRandom(self):
        # Store the old system name here
        self._storeClusterOldSystemName()
        testList = self._generateRandomSystemName()
        for i in xrange(len(testMaster.clusterTestServerList)):
            self._setSystemName(testMaster.clusterTestServerList[i], testList[i])

    def _setClusterToMerge(self):
        for s in testMaster.clusterTestServerList:
            self._setSystemName(s,self._mergeTestSystemName)

    def _rollbackSystemName(self):
        for i in xrange(len(testMaster.clusterTestServerList)):
            self._setSystemName(testMaster.clusterTestServerList[i], self._oldSystemName[i])


class PrepareServerStatus(BasicGenerator):
    """ Represents a single server with an UNIQUE system name.
    After we initialize this server, we will make it executes certain
    type of random data generation, after such generation, the server
    will have different states with other servers
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

        def cameraFunc(num):
            return CameraDataGenerator().generateCameraData(num, addr)

        def userFunc(num):
            return UserDataGenerator().generateUserData(num)

        def mediaServerFunc(num):
            return MediaServerGenerator().generateMediaServerData(num)

        return [("saveCameras",cameraFunc),
                ("saveUser",userFunc),
                ("saveMediaServer",mediaServerFunc)]

    def _sendRequest(self,addr,method,d):
        url = "http://%s/ec2/%s" % (addr, method)
        try:
            sendRequest(self._mergeTest._lock, url, d, notify=True)
        except TestRequestError as err:
            return (False,"%s at %s failed with %s" % (method, addr, err.errMessage))
        return (True,"")

    def _generateRandomStates(self,addr):
        api_list = self._generateDataAndAPIList(addr)
        for api in api_list:
            num = random.randint(self._minData,self._maxData)
            data_list = api[1](num)
            for data in data_list:
                ret, reason = self._sendRequest(addr,api[0],data[0])
                if ret == False:
                    return (ret, reason)
                testMaster.unittestRollback.addOperations(api[0], addr, data[1])
        return (True,"")

    def main(self,addr):
        ret,reason = self._generateRandomStates(addr)
        if ret == False:
            raise Exception("Cannot generate random states: %s" % (reason))


# This class is used to control the whole merge test
class MergeTest_Resource(MergeTestBase):
    _lock = threading.Lock()

    def _prolog(self):
        print "Merge test prolog : Test whether all servers you specify has the identical system name"
        oldSystemName = None
        oldSystemNameAddr = None

        # Testing whether all the cluster server has identical system name
        for s in testMaster.clusterTestServerList:
            print "Connection to http://%s/ec2/testConnection" % (s)
            response = urllib2.urlopen("http://%s/ec2/testConnection" % (s))
            if response.getcode() != 200:
                return False
            jobj = SafeJsonLoads(response.read(), s, 'testConnection')
            if jobj is None:
                return False
            if oldSystemName == None:
                oldSystemName = jobj["systemName"]
                oldSystemNameAddr = s
            else:
                systemName = jobj["systemName"]
                if systemName != oldSystemName:
                    print "The merge test cannot start: different system names!"
                    print "Server %s - '%s'; server %s - '%s'" % (
                        oldSystemName, oldSystemNameAddr, s, jobj["systemName"])
                    print "Please make all the server has identical system name before running merge test"
                    return False
            response.close()

        print "Merge test prolog pass"
        return True

    def _epilog(self):
        print "Merge test epilog, change all servers system name back to its original one"
        self._rollbackSystemName()
        print "Merge test epilog done"

    # First phase will make each server has its own status
    # and also its unique system name there

    def _phase1(self):
        print "Merge test phase1: generate UNIQUE system name for each server and do modification"
        # 1.  Set cluster system name to random name
        self._setClusterSystemRandom()

        # 2.  Start to generate server status and data
        worker = ClusterWorker(testMaster.threadNumber, len(testMaster.clusterTestServerList))

        for s in testMaster.clusterTestServerList:
            worker.enqueue(PrepareServerStatus(self).main,(s,))

        worker.join()
        print "Merge test phase1 done, now sleep %s seconds and wait for sync" % self._mergeTestTimeout
        time.sleep(self._mergeTestTimeout)

    def _phase2(self):
        print "Merge test phase2: set ALL the servers with system name: %s" % self._mergeTestSystemName
        self._setClusterToMerge()
        print "Merge test phase2: wait %s seconds for sync" % self._mergeTestTimeout
        # Wait until the synchronization time out expires
        time.sleep(self._mergeTestTimeout)
        # Do the status checking of _ALL_ API
        testMaster.checkMethodStatusConsistent(PrepareServerStatus.getterAPI)  # raises on errors
        print "Merge test phase2 done"
        return (True,"")

    def test(self):
        print "================================\n"
        print "Server Merge Test: Resource Start\n"
        try:
            if not self._prolog():
                print "FAIL: Merge Test: Resource prolog failed!"
                return False
            self._phase1()
            ret,reason = self._phase2()
            if not ret:
                print "FAIL: %s" % reason
            self._epilog()
        except Exception as err:
            print "FAIL: %s" % (err,)
            ret = False
        print "Server Merge Test: Resource End%s\n" % ('' if ret else ": test FAILED")
        print "================================\n"
        return ret

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
        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

    def _setUpClusterAuthentication(self, password):
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for s in testMaster.clusterTestServerList:
            ManagerAddPassword(passman, s, self._username, password)
        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

    def _restoreAuthentication(self):
        self._setUpClusterAuthentication(self._oldClusterPassword)

    def _merge(self):
        self._setClusterToMerge()
        time.sleep(self._mergeTestTimeout)

    def _checkPassword(self,pwd,old_server,login_server):
        print "Password:%s that initially modified on server:%s can log on to server:%s in new cluster"%(pwd,old_server,login_server)
        print "Notes,the above server can be the same one, however since this test is after merge, it still makes sense"
        print "Now test whether it works on the cluster"
        for s in testMaster.clusterTestServerList:
            if s == login_server:
                continue
            else:
                response = None
                try:
                    response = urllib2.urlopen("http://%s/ec2/testConnection"%(s))
                except urllib2.URLError,e:
                    print "This password cannot log on server:%s"%(s)
                    print "This means this password can be used partially on cluster which is not supposed to happen"
                    print "The cluster is not synchronized after merge"
                    print "Error:%s"%(e)
                    return False

        print "This password can be used on the whole cluster"
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
        print "No password is found while probing the cluster"
        print "This means after the merge,all the password originally on each server CANNOT be used to log on any server after merge"
        print "This means cluster is not synchronized"
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
            print "Fail: " + str(err)
            return False
        return True

    def _checkOnline(self,uidset,responseObj,serverAddr):
        if responseObj is None:
            return False
        for ele in responseObj:
            if ele["id"] in uidset:
                if ele["status"] != "Online":
                    # report the status
                    print "Login at server:%s"%(serverAddr)
                    print "The server:(%s) with name:%s and id:%s status is Offline"%(ele["networkAddresses"],ele["name"],ele["id"])
                    print "It should be Online after the merge"
                    print "Status check failed"
                    return False
        print "Status check for server:%s pass!"%(serverAddr)
        return True

    def _checkAllOnline(self):
        uidSet = set()
        # Set up the UID set for each registered server
        for uid in testMaster.clusterTestServerUUIDList:
            uidSet.add(uid[0])

        # For each server test whether they work or not
        for s in testMaster.clusterTestServerList:
            print "Connection to http://%s/ec2/getMediaServersEx?format=json"%(s)
            response = urllib2.urlopen("http://%s/ec2/getMediaServersEx?format=json"%(s))
            if response.getcode() != 200:
                print "Connection failed with HTTP code:%d"%(response.getcode())
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
                print "Connection http://%s/ec2/saveUsers failed"%(ser)
                print "Cannot set admin password:%s to server:%s"%(pwd,ser)
            return False

        if response.getcode() != 200:
            response.close()
            if verbose:
                print "Connection http://%s/ec2/saveUsers failed"%(ser)
                print "Cannot set admin password:%s to server:%s"%(pwd,ser)
            return False
        else:
            response.close()
            return True

    # This rollback is bit of tricky since it NEEDS to rollback partial password change
    def _rollbackPartialPasswordChange(self,pwdlist):
        # Now rollback the newAuth part of the list
        for entry in pwdlist:
            if not self._setAdminPassword(entry[0],self._oldClusterPassword):
                print "----------------------------------------------------------------------------------"
                print "+++++++++++++++++++++++++++++++++++ IMPORTANT ++++++++++++++++++++++++++++++++++++"
                print "Server:%s admin password cannot rollback,please set it back manually!"%(entry[0])
                print "It's current password is:%s"%(entry[1])
                print "It's old password is:%s"(self._oldClusterPassword)
                print "----------------------------------------------------------------------------------"
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
            print "There're some server's admin password I cannot prob and rollback"
            print "Since it is a failover rollback,I cannot guarantee that I can rollback the whole cluster"
            print "There're possible bugs in the cluster that make the automatic rollback impossible"
            print "The following server has _UNKNOWN_ password now"
            for ser in testMaster.clusterTestServerList:
                if ser not in addrSet:
                    print "The server:%s has _UNKNOWN_ password for admin"%(ser)
            return False
        else:
            return True

    def _failRollback(self):
        print "==========================================="
        print "Start Failover Rollback"
        print "This rollback will _ONLY_ happen when the merge test failed"
        print "This rollback cannot guarantee that it will rollback everything"
        print "Detail information will be reported during the rollback"
        if self._failRollbackPassword():
            self._restoreAuthentication()
            self._rollbackSystemName()
            print "Failover Rollback Done!"
        else:
            print "Failover Rollback Failed!"
        print "==========================================="

    def test(self):
        print "==========================================="
        print "Merge Test:Admin Password Test Start!"
        # At first, we fetch each system's admin information
        if self._fetchAdmin() is None:
            print "Merge Test:Fetch Admins list failed"
            return False
        # Change each system into different system name
        print "Now set each server node into different and UNIQUE system name\n"
        self._setClusterSystemRandom()
        # Change the password of _EACH_ servers
        print "Now change each server node's admin password to a UNIQUE password\n"
        if not self._changePassword():
            print "Merge Test:Admin Password Test Failed"
            return False
        print "Now set the system name back to mergeTest and wait for the merge\n"
        self._merge()
        # Now start to probing the password
        print "Start to prob one of the possible password that can be used to LOG to the cluster\n"
        if not self._probePassword():
            print "Merge Test:Admin Password Test Failed"
            self._failRollback()
            return False
        print "Check all the server status\n"
        # Now start to check the status
        if not self._checkAllServerStatus():
            print "Merge Test:Admin Password Test Failed"
            self._failRollback()
            return False
        print "Check all server is Online or not"
        if not self._checkAllOnline():
            print "Merge Test:Admin Password Test Failed"
            self._failRollback()
            return False

        print "Lastly we do rollback\n"
        self._rollback()

        print "Merge Test:Admin Password Test Pass!"
        print "==========================================="
        return True


class MergeTest(object):

    def __init__(self, needCleanUp=False):
        self._CleanUp = needCleanUp

    def run(self):
        try:
            if not MergeTest_Resource().test():
                return False
            # The following merge test ALWAYS fail and I don't know it is my problem or not
            # Current it is disabled and you could use a seperate command line to run it
            #MergeTest_AdminPassword().test()
            return True
        finally:
            if self._CleanUp:
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
            print "%s failed: %s" % (methodName, err.message)
        else:
            print "%s OK" % (methodName)

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
    if argc not in (2, 3):
        return (False,"2/1 parameters are needed")

    l = argv[1].split('=')

    if l[0] != '--add' and l[0] != '--remove':
        return (False,"Unknown first parameter options")

    t = globals()["%sOperation" % (l[1])]

    if t == None:
        return (False,"Unknown target operations:%s" % (l[1]))
    else:
        t = t()

    if l[0] == '--add':
        if argc != 3 :
            return (False,"--add must have --count option")
        l = argv[2].split('=')
        if l[0] == '--count':
            num = int(l[1])
            if num <= 0 :
                return (False,"--count must be positive integer")
            if t.add(num) == False:
                return (False,"cannot perform add operation")
        else:
            return (False,"--add can only have --count options")
    elif l[0] == '--remove':
        if argc == 3:
            l = argv[2].split('=')
            if l[0] == '--id':
                if t.remove(l[1]) == False:
                    return (False,"cannot perform remove UID operation")
            elif l[0] == '--fake':
                if t.removeAllFake() == False:
                    return (False,"cannot perform remove UID operation")
            else:
                return (False,"--remove can only have --id options")
        elif argc == 2:
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
            if type[0] :
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
        print "Start to prepare performance data"
        print "Please wait patiently"
        self._initPerfList(ret)
        # start the thread pool to run the performance test
        print "======================================"
        print "Performance Test Start"
        print "Hit CTRL+C to interrupt it"
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
        print "==================================="
        print "Performance Test Done"
        for key,value in self._statics.iteritems():
            print "---------------------------------"
            print "Server:%s" % (key)
            print "Resource Create Success: %d" % (value.createOK)
            print "Resource Create Fail:    %d" % (value.createFail)
            print "Resource Update Success: %d" % (value.updateOK)
            print "Resource Update Fail:    %d" % (value.updateFail)
            print "Resource Remove Success: %d" % (value.removeOK)
            print "Resource Remove Fail:    %d" % (value.removeFail)
            print "---------------------------------"
        print "===================================="


def runPerfTest(argv):
    l = argv[2].split('=')
    PerfTest().run(l[1])
    doCleanUp()


def doCleanUp(reinit=False):
    selection = '' if testMaster.auto_rollback else 'x'
    if not testMaster.auto_rollback:
        try :
            selection = raw_input("Press Enter to continue ROLLBACK or press x to SKIP it...")
        except:
            pass

    if not selection.startswith('x'):
        print "Starting rollback, do not interrupt!"
        testMaster.unittestRollback.doRollback()
        print "ROLLBACK DONE"
    else:
        print "Rollback skipped, you could use --recover to perform rollback later"
    if reinit:
        testMaster.init_rollback()


#TODO: make use of it!
def print_tests(suit, shift='    '):
    for test in suit:
        if isinstance(test, unittest.TestSuite):
            print "DEBUG:%s[%s]:" % (shift, type(test))
            print_tests(test, shift+'    ')
        else:
            print "DEBUG:%s%s" % (shift, test)


def CallTest(testClass):
    ###if not testMaster.openerReady:
    ###    testMaster.setUpPassword()
    # this print is used by FunctestParser.parse_timesync_start
    print "%s suits: %s" % (testClass.__name__, ', '.join(testClass.iter_suites()))
    return RunBoxTests(testClass, testMaster.getConfig())


SimpleTestKeys = {
    '--sys-name': SystemNameTest,
    '--rtsp-test': RtspTestSuit,
    '--rtsp-perf': RtspPerf,
    '--rtsp-stream': RtspStreamTest,
}
# These are the old legasy tests, just organized a bit

# Tests to be run on the vargant boxes, separately or within the autotest sequence
BoxTestKeys = {
    '--timesync': TimeSyncTest,
    '--ts-noinet': TimeSyncNoInetTest,
    '--ts-inet': TimeSyncWithInetTest,
    '--bstorage': BackupStorageTest,
    '--msarch': MultiserverArchiveTest,
    '--natcon': NatConnectionTest,
    '--stream': StreamingTest,
    '--hlso': HlsOnlyTest,
    '--dbup': DBTest,
    '--boxtests': None,
}


def RunByAutotest(arg0):
    """
    Used when this script is called by the autotesting script sauto.py
    :param arg0: str
    It is passed to unittest.main() to avoid automatical usage of sys.argv
    """
    testMaster.auto_rollback = True
    #config = testMaster.getConfig()
    need_rollback = True
    try:
        print "" # FIXME add startup message
        ret, reason = testMaster.init(notest=True)
        if not ret:
            print "Failed to initialize the cluster test object: %s" % (reason)
            return False
        config = testMaster.getConfig()
        with LegacyTestWrapper(config):
            if not testMaster.testConnection():
                print "Connection test failed"
                return False
            ret, reason = testMaster.initial_tests()
            if ret == False:
                print "The initial cluster test failed: %s" % (reason)
                return False
            print "Basic functional tests start"
            the_test = unittest.main(module=legacy_main, exit=False, argv=[arg0])
            if the_test.result.wasSuccessful():
                print "Basic functional tests end"
                if testMaster.unittestRollback:
                    doCleanUp(reinit=True)
                MergeTest().run()
                SystemNameTest(config).run()
            else:
                print "Basic functional test FAILED"
            if testMaster.unittestRollback:
                doCleanUp()
                need_rollback = False
            time.sleep(4)
            ProxyTest(*config.rtget('ServerList')[0:2]).run()
    except Exception as err:
        print "FAIL: the main functests failed with error: %s" % (err,)
    finally:
        if need_rollback and testMaster.unittestRollback:
            doCleanUp()
    if not testMaster.do_main_only:
        if not testMaster.skip_timesync:
            CallTest(TimeSyncTest)
        if not testMaster.skip_backup:
            CallTest(BackupStorageTest)
        if not testMaster.skip_mservarc:
            CallTest(MultiserverArchiveTest)
        if not testMaster.skip_streming:
            CallTest(StreamingTest)
        if not testMaster.skip_dbup:
            CallTest(DBTest)
    print "\nALL AUTOMATIC TEST ARE DONE\n"


def BoxTestsRun(key):
    testMaster.init(notest=True)
    if key == '--boxtests':
        ok = True
        if not CallTest(TimeSyncTest): ok = False
        if not CallTest(BackupStorageTest): ok = False
        if not CallTest(MultiserverArchiveTest): ok = False
        if not CallTest(StreamingTest): ok = False
        if not CallTest(DBTest): ok = False
        return ok
    else:
        return CallTest(BoxTestKeys[key])


def LegacyTests(only = False):
    _argv = argv[:]
    del _argv[1:(3 if only else 2)]
    with testMaster.unittestRollback:
        the_test = unittest.main(module=legacy_main, exit=False, argv=_argv)
    #doCleanUp(reinit=True)

    if the_test.result.wasSuccessful():
        print "Main tests passed OK"
        if (not only):
            with testMaster.unittestRollback:
                mergeOk = MergeTest().run()
            if mergeOk:
                with testMaster.unittestRollback:
                    SystemNameTest(testMaster.getConfig()).run()
    #doCleanUp()


def DoTests(argv):
    print "The automatic test starts, please wait for checking cluster status, test connection and APIs and do proper rollback..."
    # initialize cluster test environment

    argc = len(argv)

    if argc == 2 and argv[1] in BoxTestKeys:
        # box-tests can run without complete testMaster.init(), since they reinitialize mediaserver
        return BoxTestsRun(argv[1])

    ret, reason = testMaster.init()
    if not ret:
        print "Failed to initialize the cluster test object: %s" % (reason)
        return False

    if argc == 2 and argv[1] in SimpleTestKeys:
        return SimpleTestKeys[argv[1]](testMaster.getConfig()).run()

    ret, reason = testMaster.initial_tests()
    if ret == False:
        print "The initial cluster test failed: %s" % (reason)
        return False

    if argc == 2 and argv[1] == '--sync':
        return True # done here, since we just need to test whether
               # all the servers are on the same page

    if argc == 2 and argv[1] == '--proxy':
        ProxyTest(*testMaster.getConfig().rtget('ServerList')[0:2]).run()
        #FIXME no result code returning!

    if argc >= 2 and argv[1] == '--legacy':
        LegacyTests(argv[2] == '--only' if argc >= 3 else False)
        #FIXME no result code returning!

    elif argc == 2 and argv[1] == '--main':
        rc = LegacyTests()
        time.sleep(3)
        ProxyTest(*testMaster.getConfig().rtget('ServerList')[0:2]).run()

        print "\nALL AUTOMATIC TEST ARE DONE\n"
        #FIXME no result code returning!

    elif (argc == 2 or argc == 3) and argv[1] == '--clear':
        if argc == 3:
            if argv[2] == '--fake':
                doClearAll(True)
            else:
                print "Unknown option: %s in --clear" % (argv[2])
        else:
            doClearAll(False)
        testMaster.unittestRollback.removeRollbackDB()
        #FIXME no result code returning!

    elif argc == 2 and argv[1] == '--perf':
        PerfTest().start()
        doCleanUp()
        #FIXME no result code returning!

    else:
        if argv[1] == '--merge-test':
            MergeTest(True).run()
        elif argv[1] == '--merge-admin':
            MergeTest_AdminPassword().test()
            testMaster.unittestRollback.removeRollbackDB()
        elif argc == 3 and argv[1] == '--perf':
            runPerfTest()
        else:
            runMiscFunction(argc, argv)
        #FIXME no result code returning!


if __name__ == '__main__':
    reload(sys)
    sys.setdefaultencoding('utf8')
    argv = testMaster.preparseArgs(sys.argv)
    if len(argv) == 1:  # called from auto.py, using boxes which are created, but servers not started
        RunByAutotest(argv[0])
    elif len(argv) >= 2 and argv[1] in ('--help', '-h'):
        showHelp(argv)
    elif len(argv) == 2 and argv[1] == '--recover':
        UnitTestRollback(autorollback=True, nocreate=True)
    else:
        if not DoTests(argv):
            sys.exit(1)
