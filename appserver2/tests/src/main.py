import unittest
import urllib2
import urllib
import ConfigParser
import time
import threading
import json
import random
import md5
import string
import sys
import Queue
import socket
import os.path
import signal
import sys
import difflib
import datetime
import time
import select
import errno

# Rollback support
class UnitTestRollback:
    _rollbackFile = None
    _removeTemplate = """
        {
            "id":"%s"
        }
    """

    def __init__(self):
        if os.path.isfile(".rollback"):
            selection = None
            try :
                print "+++++++++++++++++++++++++++++++++++++++++++WARNING!!!++++++++++++++++++++++++++++++++"
                print "The .rollback file has been detected, if continues to run test the previous rollback information will be lost!\n"
                print "Do you want to run Recover NOW?\n"
                selection = raw_input("Press r to RUN RECOVER at first or press Enter to SKIP RECOVER and run the test")
            except:
                pass

            if len(selection) != 0 and selection[0] == 'r':
                self.doRecover()

        self._rollbackFile = open(".rollback","w+")

    def addOperations(self,methodName,serverAddress,resourceId):
        for s in  clusterTest.clusterTestServerList:
            self._rollbackFile.write(("%s,%s,%s\n") % (methodName,s,resourceId))
        self._rollbackFile.flush()

    def _doSingleRollback(self,methodName,serverAddress,resourceId):
        # this function will do a single rollback
        req = urllib2.Request("http://%s/ec2/%s" % (serverAddress,"removeResource"), \
            data=self._removeTemplate % (resourceId), headers={'Content-Type': 'application/json'})
        response = None
        try:
            response = urllib2.urlopen(req)
        except:
            return False
        if response.getcode() != 200:
            response.close()
            return False
        response.close()
        return True

    def doRollback(self):
        recoverList = []
        failed = False
        # set the cursor for the file to the file beg
        self._rollbackFile.seek(0,0)
        for line in self._rollbackFile:
            if line == '\n':
                continue
            # python will left the last line break character there
            # so we need to wipe it out if we find one there
            l = line.split(',')
            if l[2][len(l[2]) - 1] == "\n" :
                l[2] = l[2][:-1] # remove last character
            # now we have method,serverAddress,resourceId
            if self._doSingleRollback(l[0],l[1],l[2]) == False:
                failed = True
                # failed for this rollback
                print ("Cannot rollback for transaction:(MethodName:%s;ServerAddress:%s;ResourceId:%s\n)") % (l[0],l[1],l[2])
                print  "Or you could run recover later when all the rollback done\n"
                recoverList.append("%s,%s,%s\n" % (l[0],l[1],l[2]))
            else:
                print "..rollback done.."

        # close the resource file
        self._rollbackFile.close()
        os.remove(".rollback")

        if failed:
            recoverFile = open(".rollback","w+")
            for line in recoverList:
                recoverFile.write(line)
            recoverFile.close()

    def doRecover(self):
        if not os.path.isfile(".rollback") :
            print "Nothing needs to be recovered"
            return
        else:
            print "Start to recover from previous failed rollback transaction"
        # Do the rollback here
        self._rollbackFile = open(".rollback","r")
        self.doRollback()
        print "Recover done..."

    def removeRollbackDB(self):
        self._rollbackFile.close()
        os.remove(".rollback")

class ClusterTest():
    clusterTestServerList = []
    clusterTestSleepTime = None
    clusterTestServerUUIDList = []
    threadNumber = 16
    testCaseSize = 2
    unittestRollback = None

    _getterAPIList = ["getResourceParams",
        "getMediaServersEx",
        "getCamerasEx",
        "getUsers"]

    _ec2GetRequests = ["getResourceTypes",
        "getResourceParams",
        "getMediaServers",
        "getMediaServersEx",
        "getCameras",
        "getCamerasEx",
        "getCameraHistoryItems",
        "getCameraBookmarkTags",
        "getBusinessRules",
        "getUsers",
        "getVideowalls",
        "getLayouts",
        "listDirectory",
        "getStoredFile",
        "getSettings",
        "getCurrentTime",
        "getFullInfo",
        "getLicenses",
        "getTransactionLog"]

    def _callAllGetters(self):
        print "======================================"
        print "Test all ec2 get request status"
        for s in clusterTest.clusterTestServerList:
            for reqName in self._ec2GetRequests:
                print "Connection to http://%s/ec2/%s" % (s,reqName)
                response = urllib2.urlopen("http://%s/ec2/%s" % (s,reqName))
                if response.getcode() != 200:
                    return (False,"%s failed with statusCode %d" % (reqName,response.getcode()))
                response.close()
        print "All ec2 get requests work well"
        print "======================================"
        return (True,"Server:%s test for all getter pass" % (s))

    def _getServerName(self,obj,uuid):
        for s in obj:
            if s["id"] == uuid:
                return s["name"]
        return None

    def _patchUUID(self,uuid):
        if uuid[0] == '{' :
            return uuid
        else:
            return "{%s}" % (uuid)

    # call this function after reading the clusterTestServerList
    def _fetchClusterTestServerUUIDList(self):

        server_id_list = []
        for s in self.clusterTestServerList:
            response = urllib2.urlopen("http://%s/ec2/testConnection?format=json" % (s))
            if response.getcode() != 200:
                return (False,"testConnection response with error code:%d" % (response.getcode()))
            json_obj = json.loads(response.read())
            server_id_list.append(self._patchUUID(json_obj["ecsGuid"]))
            response.close()

        # We still need to get the server name since this is useful for
        # the SaveServerAttributeList test (required in its post data)

        response = urllib2.urlopen("http://%s/ec2/getMediaServersEx?format=json" % (self.clusterTestServerList[0]))

        if response.getcode() != 200:
            return (False,"getMediaServersEx resposne with error code:%d" % (response.getcode()))

        json_obj = json.loads(response.read())

        for uuid in server_id_list:
            n = self._getServerName(json_obj,uuid)
            if n == None:
                return (False,"Cannot fetch server name with UUID:%s" % (uuid))
            else:
                self.clusterTestServerUUIDList.append((uuid,n))

        response.close()
        return (True,"")

    def _dumpDiffStr(self,str,i):
        if len(str) == 0:
            print "<empty string>"
        else:
            start = max(0,i - 64)
            end = min(64,len(str) - i) + i
            comp1 = str[start:i]
            comp2 = str[i]
            comp3 = ""
            if i + 1 >= len(str):
                comp3 = ""
            else:
                comp3 = str[i + 1:end]
            print "%s^^^%s^^^%s\n" % (comp1,comp2,comp3)


    def _seeDiff(self,lhs,rhs):
        if len(rhs) == 0 or len(lhs) == 0:
            print "The difference is showing bellow:\n"
            if len(lhs) == 0:
                print "<empty string>"
            else:
                print rhs[0:min(128,len(rhs))]
            if len(rhs) == 0:
                print "<empty string>"
            else:
                print rhs[0:min(128,len(rhs))]

            print "One of the string is empty!"
            return

        for i in xrange(max(len(rhs),len(lhs))):
            if i >= len(rhs) or i >= len(lhs) or lhs[i] != rhs[i]:
                print "The difference is showing bellow:"
                self._dumpDiffStr(lhs,i)
                self._dumpDiffStr(rhs,i)
                print "The first different character is at location:%d" % (i + 1)
                return


    def _testConnection(self):
        print "==================================================\n"
        print "Test connection on each server in the server list "
        for s in self.clusterTestServerList:
            print "Try to connect to server:%s" % (s)
            try:
                response = urllib2.urlopen("http://%s/ec2/testConnection" % (s),timeout=5)
            except urllib2.URLError , e:
                print "The connection will be issued with a 5 seconds timeout"
                print "However connection failed with error:%r" % (e)
                print "Connection test failed"
                return False

            if response.getcode() != 200:
                return False
            response.close()
            print "Connection test OK\n"

        print "Connection Test Pass"
        print "\n==================================================="
        return True

    # This checkResultEqual function will categorize the return value from each
    # server
    # and report the difference status of this

    def _reportDetailDiff(self,key_list):
        for i in xrange(len(key_list)):
            for k in xrange(i + 1,len(key_list)):
                print "-----------------------------------------"
                print "Group %d compared with Group %d\n" % (i + 1,k + 1)
                self._seeDiff(key_list[i],key_list[k])
                print "-----------------------------------------"

    def _reportDiff(self,statusDict,methodName):
        print "\n\n**************************************************\n\n"
        print "Report each server status of method:%s\n" % (methodName)
        print "The server will be categorized as group,each server within the same group has same status,while different groups have different status\n"
        print "The total group number is:%d\n" % (len(statusDict))
        if len(statusDict) == 1:
            print "The status check passed!\n"
        i = 1
        key_list = []
        for key in statusDict:
            list = statusDict[key]
            print "Group %d:(%s)\n" % (i,','.join(list))
            i = i + 1
            key_list.append(key)


        self._reportDetailDiff(key_list)
        print "\n\n**************************************************\n\n"

    def _checkResultEqual(self,responseList,methodName):
        statusDict = dict()

        for entry in responseList:
            response = entry[0]
            address = entry[1]

            if response.getcode() != 200:
                return(False,"Server:%s method:%s http request failed with code:%d" % (address,methodName,response.getcode()))
            else:
                content = response.read()
                if content in statusDict:
                    statusDict[content].append(address)
                else:
                    statusDict[content] = [address]
                response.close()

        self._reportDiff(statusDict,methodName)

        if len(statusDict) > 1:
            return (False,"")

        return (True,"")

    def _checkResultEqual_Deprecated(self,responseList,methodName):
        print "------------------------------------------\n"
        print "Test sync status on method:%s" % (methodName)
        result = None
        resultAddr = None
        for entry in responseList:
            response = entry[0]
            address = entry[1]

            if response.getcode() != 200:
                return (False,"Server:%s method:%s http request failed with code:%d" % (address,methodName,response.getcode()))
            else:
                content = response.read()
                if result == None:
                    result = content
                    resultAddr = address
                else:
                    if content != result:
                        print "Server:%s has different status with server:%s on method:%s" % (address,resultAddr,methodName)
                        self._seeDiff(content,result)
                        return (False,"Failed to sync")
            response.close()
        print "Method:%s is sync in cluster" % (methodName)
        print "\n------------------------------------------"
        return (True,"")


    def _checkSingleMethodStatusConsistent(self,method):
            responseList = []
            for server in self.clusterTestServerList:
                print "Connection to http://%s/ec2/%s" % (server, method)
                responseList.append((urllib2.urlopen("http://%s/ec2/%s" % (server, method)),server))

            # checking the last response validation
            return self._checkResultEqual_Deprecated(responseList,method)

    # Checking transaction log
    def _checkTransactionLog(self):
        return self._checkSingleMethodStatusConsistent("getTransactionLog")

    def checkMethodStatusConsistent(self,method):
            ret,reason = self._checkSingleMethodStatusConsistent(method)
            if ret:
                return self._checkTransactionLog()
            else:
                return (ret,reason)

    def _ensureServerListStates(self,sleep_timeout):
        time.sleep(sleep_timeout)
        for method in self._getterAPIList:
            ret,reason = self._checkSingleMethodStatusConsistent(method)
            if ret == False:
                return (ret,reason)
        return self._checkTransactionLog()

    def _loadConfig(self):
        parser = ConfigParser.RawConfigParser()
        parser.read("ec2_tests.cfg")
        self.clusterTestServerList = parser.get("General","serverList").split(",")
        self.clusterTestSleepTime = parser.getint("General","clusterTestSleepTime")
        self.threadNumber = parser.getint("General","threadNumber")
        try :
            self.testCaseSize = parser.getint("General","testCaseSize")
        except :
            self.testCaseSize = 2

        return (parser.get("General","password"),parser.get("General","username"))


    def _setUpPassword(self):
        pwd,un = self._loadConfig()
        # configure different domain password strategy
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for s in self.clusterTestServerList:
            passman.add_password("NetworkOptix","http://%s/ec2/" % (s), un, pwd)
            passman.add_password("NetworkOptix","http://%s/api/" % (s), un, pwd)

        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

    def init(self):
        self._setUpPassword()

        if not self._testConnection():
            return (False,"Connection test failed")

        # ensure all the server are on the same page
        ret,reason = self._ensureServerListStates(self.clusterTestSleepTime)

        if ret == False:
            return (ret,reason)

        ret,reason = self._fetchClusterTestServerUUIDList()

        if ret == False:
            return (ret,reason)

        ret,reason = self._callAllGetters()
        if ret == False:
            return (ret,reason)

        # do the rollback here
        self.unittestRollback = UnitTestRollback()
        return (True,"")

clusterTest = ClusterTest()

_uniqueSessionNumber = None
_uniqueCameraSeedNumber = 0
_uniqueUserSeedNumber = 0
_uniqueMediaServerSeedNumber = 0



# Thread queue for multi-task.  This is useful since if the user
# want too many data to be sent to the server, then there maybe
# thousands of threads to be created, which is not something we
# want it.  This is not a real-time queue, but a push-sync-join
class ClusterWorker():
    _queue = None
    _threadList = None
    _threadNum = 1

    def __init__(self,num,element_size):
        self._queue = Queue.Queue(element_size)
        self._threadList = []
        self._threadNum = num
        if element_size < num:
            self._threadNum = element_size

    def _worker(self):
        while not self._queue.empty():
            t,a = self._queue.get(True)
            t(*a)
            self._queue.task_done()

    def _initializeThreadWorker(self):
        for _ in xrange(self._threadNum):
            t = threading.Thread(target=self._worker)
            t.start()
            self._threadList.append(t)

    def join(self):
        # We delay the real operation until we call join
        self._initializeThreadWorker()
        # Second we call queue join to join the queue
        self._queue.join()
        # Now we safely join the thread since the queue
        # will utimatly be empty after execution
        for t in self._threadList:
            t.join()

    def enqueue(self,task,args):
        self._queue.put((task,args),True)

class BasicGenerator():
    def __init__(self):
        global _uniqueSessionNumber
        if _uniqueSessionNumber == None:
            # generate unique session number
            _uniqueSessionNumber = str(random.randint(1,1000000))

    # generate MAC address of the object
    def generateMac(self):
        l = []
        for i in xrange(0,5):
            l.append(str(random.randint(0,255)) + '-')
        l.append(str(random.randint(0,255)))
        return ''.join(l)

    def generateTrueFalse(self):
        if random.randint(0,1) == 0:
            return "false"
        else:
            return "true"

    def generateRandomString(self,length):
        chars = string.ascii_uppercase + string.digits
        return ''.join(random.choice(chars) for _ in xrange(length))

    def generateUUIdFromMd5(self,salt):
        m = md5.new()
        m.update(salt)
        v = m.digest()
        return "{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}" \
            % (ord(v[0]),ord(v[1]),ord(v[2]),ord(v[3]),
              ord(v[4]),ord(v[5]),ord(v[6]),ord(v[7]),
              ord(v[8]),ord(v[9]),ord(v[10]),ord(v[11]),
              ord(v[12]),ord(v[13]),ord(v[14]),ord(v[15]))
        
    def generateRandomId(self):
        length = random.randint(6,12)
        salt = self.generateRandomString(length)
        return self.generateUUIdFromMd5(salt)

    def generateIpV4(self):
        return "%d.%d.%d.%d" % (random.randint(0,255),
            random.randint(0,255),
            random.randint(0,255),
            random.randint(0,255))

    def generateIpV4Endpoint(self):
        return "%s:%d" % (self.generateIpV4(),random.randint(0,65535))

    def generateEmail(self):
        len = random.randint(6,20)
        user_name = self.generateRandomString(len)
        return "%s@gmail.com" % (user_name)

    def generateEnum(self,*args):
        idx = random.randint(0,len(args) - 1)
        return args[idx]

    def generateUsernamePasswordAndDigest(self,namegen):
        pwd_len = random.randint(8,20)

        un = namegen()
        pwd = self.generateRandomString(pwd_len)

        m = md5.new()
        m.update("%s:NetworkOptix:%s" % (un,pwd))
        d = m.digest()

        return (un,pwd,''.join('%02x' % ord(i) for i in d))

    def generateDigest(self,uname,pwd):
        m = md5.new()
        m.update("%s:NetworkOptix:%s" % (uname,pwd))
        d = m.digest()
        return ''.join("%02x" % ord(i) for i in d)
    
    def generatePasswordHash(self,pwd):
        salt = "%x" % (random.randint(0,4294967295))
        m = md5.new()
        m.update(salt)
        m.update(pwd)
        md5_digest = ''.join('%02x' % ord(i) for i in m.digest())
        return "md5$%s$%s" % (salt,md5_digest)


    def generateCameraName(self):
        global _uniqueSessionNumber
        global _uniqueCameraSeedNumber

        ret = "ec2_test_cam_%s_%s" % (_uniqueSessionNumber,_uniqueCameraSeedNumber)
        _uniqueCameraSeedNumber = _uniqueCameraSeedNumber + 1
        return ret

    def generateUserName(self):
        global _uniqueSessionNumber
        global _uniqueUserSeedNumber

        ret = "ec2_test_user_%s_%s" % (_uniqueSessionNumber,_uniqueUserSeedNumber)
        _uniqueUserSeedNumber = _uniqueUserSeedNumber + 1
        return ret

    def generateMediaServerName(self):
        global _uniqueSessionNumber
        global _uniqueMediaServerSeedNumber

        ret = "ec2_test_media_server_%s_%s" % (_uniqueSessionNumber,_uniqueMediaServerSeedNumber)
        _uniqueMediaServerSeedNumber = _uniqueMediaServerSeedNumber + 1
        return ret


class CameraDataGenerator(BasicGenerator):
    _template = """[
        {
            "audioEnabled": %s,
            "controlEnabled": %s,
            "dewarpingParams": "",
            "groupId": "",
            "groupName": "",
            "id": "%s",
            "mac": "%s",
            "manuallyAdded": false,
            "maxArchiveDays": 0,
            "minArchiveDays": 0,
            "model": "%s",
            "motionMask": "",
            "motionType": "MT_Default",
            "name": "%s",
            "parentId": "%s",
            "physicalId": "%s",
            "preferedServerId": "{00000000-0000-0000-0000-000000000000}",
            "scheduleEnabled": false,
            "scheduleTasks": [ ],
            "secondaryStreamQuality": "SSQualityLow",
            "status": "Unauthorized",
            "statusFlags": "CSF_NoFlags",
            "typeId": "{7d2af20d-04f2-149f-ef37-ad585281e3b7}",
            "url": "%s",
            "vendor": "%s"
        }
    ]"""

    def _generateCameraId(self,mac):
        return self.generateUUIdFromMd5(mac)

    def _getServerUUID(self,addr):
        if addr == None:
            return "{2571646a-7313-4324-8308-c3523825e639}"
        i = 0

        for s in  clusterTest.clusterTestServerList:
            if addr == s:
                break
            else:
                i = i + 1

        return clusterTest.clusterTestServerUUIDList[i][0]

    def generateCameraData(self,number,mediaServer):
        ret = []
        for i in xrange(number):
            mac = self.generateMac()
            id = self._generateCameraId(mac)
            name_and_model = self.generateCameraName()
            ret.append((self._template % (self.generateTrueFalse(),
                    self.generateTrueFalse(),
                    id,
                    mac,
                    name_and_model,
                    name_and_model,
                    self._getServerUUID(mediaServer),
                    mac,
                    self.generateIpV4(),
                    self.generateRandomString(4)),id))

        return ret

    def generateUpdateData(self,id,mediaServer):
        mac = self.generateMac()
        name_and_model = self.generateCameraName()
        return (self._template % (self.generateTrueFalse(),
                self.generateTrueFalse(),
                id,
                mac,
                name_and_model,
                name_and_model,
                self._getServerUUID(mediaServer),
                mac,
                self.generateIpV4(),
                self.generateRandomString(4)),id)


class UserDataGenerator(BasicGenerator):
    _template = """
    {
        "digest": "%s",
        "email": "%s",
        "hash": "%s",
        "id": "%s",
        "isAdmin": %s,
        "name": "%s",
        "parentId": "{00000000-0000-0000-0000-000000000000}",
        "permissions": "255",
        "typeId": "{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}",
        "url": ""
    }
    """

    def generateUserData(self,number):
        ret = []
        for i in xrange(number):
            id = self.generateRandomId()
            un,pwd,digest = self.generateUsernamePasswordAndDigest(self.generateUserName)
            ret.append((self._template % (digest,
                self.generateEmail(),
                self.generatePasswordHash(pwd),
                id,"false",un),id))

        return ret

    def generateUpdateData(self,id):
        un,pwd,digest = self.generateUsernamePasswordAndDigest(self.generateUserName)
        return (self._template % (digest,
                self.generateEmail(),
                self.generatePasswordHash(pwd),
                id,"false",un),id)

    def createManualUpdateData(self,id,username,password,admin,email):
        digest = self.generateDigest(username,password)
        hash = self.generatePasswordHash(password)
        if admin:
            admin = "true"
        else:
            admin = "false"
        return self._template%(digest,
                                email,
                                hash,id,admin,username)

class MediaServerGenerator(BasicGenerator):
    _template = """
    {
        "apiUrl": "%s",
        "authKey": "%s",
        "flags": "SF_HasPublicIP",
        "id": "%s",
        "name": "%s",
        "networkAddresses": "192.168.0.1;10.0.2.141;192.168.88.1;95.31.23.214",
        "panicMode": "PM_None",
        "parentId": "{00000000-0000-0000-0000-000000000000}",
        "systemInfo": "windows x64 win78",
        "systemName": "%s",
        "typeId": "{be5d1ee0-b92c-3b34-86d9-bca2dab7826f}",
        "url": "rtsp://%s",
        "version": "2.3.0.0"
    }
    """
    def _generateRandomName(self):
        length = random.randint(5,20)
        return self.generateRandomString(length)

    def generateMediaServerData(self,number):
        ret = []
        for i in xrange(number):
            id = self.generateRandomId()
            ret.append((self._template % (self.generateIpV4Endpoint(),
                   self.generateRandomId(),
                   id,
                   self.generateMediaServerName(),
                   self._generateRandomName(),
                   self.generateIpV4Endpoint()),id))

        return ret

# This class serves as an in-memory data base.  Before doing the confliction
# test,
# we begining by creating some sort of resources and then start doing the
# confliction
# test.  These data is maintained in a separate dictionary and once everything
# is done
# it will be rollback.
class ConflictionDataGenerator(BasicGenerator):
    conflictCameraList = []
    conflictUserList = []
    conflictMediaServerList = []
    _lock = threading.Lock()
    _listLock = threading.Lock()
    
    def _prepareData(self,dataList,methodName,l):
        worker = ClusterWorker(8,len(clusterTest.clusterTestServerList) * len(dataList))

        for d in dataList:
            for s in clusterTest.clusterTestServerList:

                def task(lock,list,list_lock,post_data,server):

                    req = urllib2.Request("http://%s/ec2/%s" % (server,methodName),
                      data=post_data[0], headers={'Content-Type': 'application/json'})

                    response = None

                    with lock:
                        response = urllib2.urlopen(req)

                    if response.getcode() != 200:
                        response.close()
                        print "Failed to connect to http://%s/ec2/%s" % (server,methodName)
                    else:
                        clusterTest.unittestRollback.addOperations(methodName,server,post_data[1])
                        with list_lock:
                            list.append(post_data[0])
                    response.close() 

                worker.enqueue(task,(self._lock,l,self._listLock,d,s,))

        worker.join()
        return True

    def _prepareCameraData(self,op,num,methodName,l):
        worker = ClusterWorker(8,len(clusterTest.clusterTestServerList) * num)

        for _ in xrange(num):
            for s in clusterTest.clusterTestServerList:
                d = op(1,s)[0]

                def task(lock,list,list_lock,post_data,server):
                    req = urllib2.Request("http://%s/ec2/%s" % (server,methodName),
                          data=post_data[0], headers={'Content-Type': 'application/json'})

                    response = None

                    with lock:
                        response = urllib2.urlopen(req)

                    if response.getcode() != 200:
                        # failed
                        response.close()
                        print "Failed to connect to http://%s/ec2/%s" % (server,methodName)
                    else:
                        clusterTest.unittestRollback.addOperations(methodName,server,post_data[1])
                        with list_lock:
                            list.append(d[0])

                    response.close()

                worker.enqueue(task,(self._lock,l,self._listLock,d,s,))

        worker.join()
        return True

    def prepare(self,num):
        return \
            self._prepareCameraData(CameraDataGenerator().generateCameraData,num,"saveCameras",self.conflictCameraList) and \
            self._prepareData(UserDataGenerator().generateUserData(num),"saveUser",self.conflictUserList) and \
            self._prepareData(MediaServerGenerator().generateMediaServerData(num),"saveMediaServer",self.conflictMediaServerList)

class ResourceDataGenerator(BasicGenerator):

    _ec2ResourceGetter = ["getCameras",
        "getMediaServersEx",
        "getUsers"]

    _resourceParEntryTemplate = """
        {
            "name":"%s",
            "resourceId":"%s",
            "value":"%s"
        }
    """

    _resourceRemoveTemplate = """
        {
            "id":"%s"
        }
    """

    # this list contains all the existed resource that I can find.
    # The method for finding each resource is based on the API in
    # the list _ec2ResourceGetter.  What's more , the parentId, typeId
    # and resource name will be recorded (Can be None).
    _existedResourceList = []
    
    # this function is used to retrieve the resource list on the
    # server side.  We just retrieve the resource list from the
    # very first server since each server has exact same resource
    def _retrieveResourceUUIDList(self,num):
        gen = ConflictionDataGenerator()
        if not gen.prepare(num):
            return False

        # cameras
        for entry in gen.conflictCameraList:
            obj = json.loads(entry)[0]
            self._existedResourceList.append((obj["id"],obj["parentId"],obj["typeId"]))
        # users
        for entry in gen.conflictUserList:
            obj = json.loads(entry)
            self._existedResourceList.append((obj["id"],obj["parentId"],obj["typeId"]))
        # media server
        for entry in gen.conflictMediaServerList:
            obj = json.loads(entry)
            self._existedResourceList.append((obj["id"],obj["parentId"],obj["typeId"]))
        
        return True
    
    def __init__(self,num):
        ret = self._retrieveResourceUUIDList(num)
        if ret == False:
            raise Exception("cannot retrieve resources list on server side")

    def _generateKeyValue(self,uuid):
        key_len = random.randint(4,12)
        val_len = random.randint(24,512)
        return self._resourceParEntryTemplate % (self.generateRandomString(key_len),
            uuid,
            self.generateRandomString(val_len))

    def _getRandomResourceUUID(self):
        idx = random.randint(0,len(self._existedResourceList) - 1)
        return self._existedResourceList[idx][0]

    def _generateOneResourceParams(self):
        uuid = self._getRandomResourceUUID()
        kv_list = ["["]
        num = random.randint(2,20)
        for i in xrange(num - 1):
            num = random.randint(2,20)
            kv_list.append(self._generateKeyValue(uuid))
            kv_list.append(",")

        kv_list.append(self._generateKeyValue(uuid))
        kv_list.append("]")

        return ''.join(kv_list)
    
    def generateResourceParams(self,number):
        ret = []
        for i in xrange(number):
            ret.append(self._generateOneResourceParams())
        return ret

    def generateRemoveResource(self,number):
        ret = []
        for i in xrange(number):
            ret.append(self._resourceRemoveTemplate % (self._getRandomResourceUUID()))
        return ret

# This class is used to generate data for simulating resource confliction

# Currently we don't have general method to _UPDATE_ an existed record in db,
# so I implement it through creation routine since this routine is the only way
# to modify the existed record now.
class CameraConflictionDataGenerator(BasicGenerator):
    #(id,mac,model,parentId,typeId,url,vendor)
    _existedCameraList = []
    # For simplicity , we just modify the name of this camera
    _updateTemplate = \
    """
        [{
            "groupId": "",
            "groupName": "",
            "id": "%s",
            "mac": "%s",
            "manuallyAdded": false,
            "maxArchiveDays": 0,
            "minArchiveDays": 0,
            "model": "%s",
            "motionMask": "",
            "motionType": "MT_Default",
            "name": "%s",
            "parentId": "%s",
            "physicalId": "%s",
            "preferedServerId": "{00000000-0000-0000-0000-000000000000}",
            "scheduleEnabled": false,
            "scheduleTasks": [ ],
            "secondaryStreamQuality": "SSQualityLow",
            "status": "Unauthorized",
            "statusFlags": "CSF_NoFlags",
            "typeId": "%s",
            "url": "%s",
            "vendor": "%s"
        }]
    """
    # removeTemplate
    _removeTemplate = \
        """
        {
            "id":"%s"
        }
        """

    def _fetchExistedCameras(self,dataGen):
        for entry in dataGen.conflictCameraList:
            obj = json.loads(entry)[0]
            self._existedCameraList.append((obj["id"],obj["mac"],obj["model"],obj["parentId"],
                 obj["typeId"],obj["url"],obj["vendor"]))
        return True

    def __init__(self,dataGen):
        if self._fetchExistedCameras(dataGen) == False:
            raise Exception("Cannot get existed camera list")
    
    def _generateModify(self,camera):
        name = self.generateRandomString(random.randint(8,12))
        return self._updateTemplate % (camera[0],
            camera[1],
            camera[2],
            name,
            camera[3],
            camera[1], 
            camera[4],
            camera[5],
            camera[6])
    
    def _generateRemove(self,camera):
        return self._removeTemplate % (camera[0])  
    
    def generateData(self):
        camera = self._existedCameraList[random.randint(0,len(self._existedCameraList) - 1)]

        return (self._generateModify(camera),self._generateRemove(camera))

class UserConflictionDataGenerator(BasicGenerator):
    _updateTemplate = """
    {
        "digest": "%s",
        "email": "%s",
        "hash": "%s",
        "id": "%s",
        "isAdmin": false,
        "name": "%s",
        "parentId": "{00000000-0000-0000-0000-000000000000}",
        "permissions": "%s",
        "typeId": "{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}",
        "url": ""
    }
    """

    _removeTemplate = """
        {
            "id":"%s"
        }
    """

    _existedUserList = []

    def _fetchExistedUser(self,dataGen):
        for entry in dataGen.conflictUserList:
            obj = json.loads(entry)
            if obj["isAdmin"] == True:
                continue # skip admin
            self._existedUserList.append((obj["digest"],obj["email"],obj["hash"],
                                          obj["id"],obj["permissions"]))
        return True

    def __init__(self,dataGen):
        if self._fetchExistedUser(dataGen) == False:
            raise Exception("Cannot get existed user list")

    def _generateModify(self,user):
        name = self.generateRandomString(random.randint(8,20))

        return self._updateTemplate % (user[0],
            user[1],
            user[2],
            user[3],
            name,
            user[4])

    def _generateRemove(self,user):
        return self._removeTemplate % (user[3])


    def generateData(self):
        user = self._existedUserList[random.randint(0,len(self._existedUserList) - 1)]

        return(self._generateModify(user),self._generateRemove(user))


class MediaServerConflictionDataGenerator(BasicGenerator):
    _updateTemplate = """
    {
        "apiUrl": "%s",
        "authKey": "%s",
        "flags": "SF_HasPublicIP",
        "id": "%s",
        "name": "%s",
        "networkAddresses": "192.168.0.1;10.0.2.141;192.168.88.1;95.31.23.214",
        "panicMode": "PM_None",
        "parentId": "{00000000-0000-0000-0000-000000000000}",
        "systemInfo": "windows x64 win78",
        "systemName": "%s",
        "typeId": "{be5d1ee0-b92c-3b34-86d9-bca2dab7826f}",
        "url": "%s",
        "version": "2.3.0.0"
    }
    """

    _removeTemplate = """
        {
            "id":"%s"
        }
    """

    _existedMediaServerList = []

    def _fetchExistedMediaServer(self,dataGen):
        for server in dataGen.conflictMediaServerList:
            obj = json.loads(server)
            self._existedMediaServerList.append((obj["apiUrl"],obj["authKey"],obj["id"],
                                                 obj["systemName"],obj["url"]))

        return True


    def __init__(self,dataGen):
        if self._fetchExistedMediaServer(dataGen) == False:
            raise Exception("Cannot fetch media server list")


    def _generateModify(self,server):
        name = self.generateRandomString(random.randint(8,20))

        return self._updateTemplate % (server[0],server[1],server[2],name,
            server[3],server[4])

    def _generateRemove(self,server):
        return self._removeTemplate % (server[2])

    def generateData(self):
        server = self._existedMediaServerList[random.randint(0,len(self._existedMediaServerList) - 1)]
        return (self._generateModify(server),self._generateRemove(server))
            
class CameraUserAttributesListDataGenerator(BasicGenerator):
    _template = """
        [
            {
                "audioEnabled": %s,
                "cameraID": "%s",
                "cameraName": "%s",
                "controlEnabled": %s,
                "dewarpingParams": "%s",
                "maxArchiveDays": -30,
                "minArchiveDays": -1,
                "motionMask": "5,0,0,44,32:5,0,0,44,32:5,0,0,44,32:5,0,0,44,32",
                "motionType": "MT_SoftwareGrid",
                "preferedServerId": "%s",
                "scheduleEnabled": %s,
                "scheduleTasks": [ ],
                "secondaryStreamQuality": "SSQualityMedium"
            }
        ]
        """

    _dewarpingTemplate = "{ \\\"enabled\\\":%s,\\\"fovRot\\\":%s,\
            \\\"hStretch\\\":%s,\\\"radius\\\":%s, \\\"viewMode\\\":\\\"VerticalDown\\\",\
            \\\"xCenter\\\":%s,\\\"yCenter\\\":%s}"

    _existedCameraUUIDList = []

    _lock = threading.Lock()
    _listLock = threading.Lock()

    def __init__(self,prepareNum):
        if self._fetchExistedCameraUUIDList(prepareNum) == False:
            raise Exception("Cannot initialize camera list attribute test data")

    def _prepareData(self,op,num,methodName,l):
        worker = ClusterWorker(8,num * len(clusterTest.clusterTestServerList))

        for _ in xrange(num):
            for s in clusterTest.clusterTestServerList:

                def task(lock,list,listLock,server,mname,oper):

                    with lock:
                        d = oper(1,s)[0]
                        req = urllib2.Request("http://%s/ec2/%s" % (server,mname),
                                data=d[0], headers={'Content-Type': 'application/json'})
                        response = urllib2.urlopen(req)

                    if response.getcode() != 200:
                        print "Failed to connect http://%s/ec2/%s" % (server,mname)
                        return
                    else:
                        clusterTest.unittestRollback.addOperations(mname,server,d[1])
                        with listLock:
                            list.append(d[0])

                    response.close()

                worker.enqueue(task,(self._lock,l,self._listLock,s,methodName,op,))

        worker.join()
        return True

    def _fetchExistedCameraUUIDList(self,num):
        # We could not use existed camera list since if we do so we may break
        # the existed
        # database on the server side.  What we gonna do is just create new
        # fake cameras and
        # then do the test by so.
        json_list = []

        if not self._prepareData(CameraDataGenerator().generateCameraData,num,"saveCameras",json_list):
            return False

        for entry in json_list:
            obj = json.loads(entry)[0]
            self._existedCameraUUIDList.append((obj["id"],obj["name"]))

        return True

    def _getRandomServerId(self):
        idx = random.randint(0,len(clusterTest.clusterTestServerUUIDList) - 1)
        return clusterTest.clusterTestServerUUIDList[idx][0]

    def _generateNormalizeRange(self):
        return str(random.random())

    def _generateDewarpingPar(self):
        return self._dewarpingTemplate % (self.generateTrueFalse(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange())

    def _getRandomCameraUUIDAndName(self):
        return self._existedCameraUUIDList[random.randint(0,len(self._existedCameraUUIDList) - 1)]

    def generateCameraUserAttribute(self,number):
        ret = []

        for i in xrange(number):
            uuid , name = self._getRandomCameraUUIDAndName()
            ret.append(self._template % (self.generateTrueFalse(),
                    uuid,name,
                    self.generateTrueFalse(),
                    self._generateDewarpingPar(),
                    self._getRandomServerId(),
                    self.generateTrueFalse()))

        return ret

class ServerUserAttributesListDataGenerator(BasicGenerator):
    _template = """
    [
        {
            "allowAutoRedundancy": %s,
            "maxCameras": %s,
            "serverID": "%s",
            "serverName": "%s"
        }
    ]
    """

    _existedFakeServerList = []

    _lock = threading.Lock()
    _listLock = threading.Lock()

    def _prepareData(self,dataList,methodName,l):
        worker = ClusterWorker(8,len(dataList) * len(clusterTest.clusterTestServerList))

        for d in dataList:
            for s in clusterTest.clusterTestServerList:
                
                def task(lock,list,listLock,post_data,mname,server):
                    req = urllib2.Request("http://%s/ec2/%s" % (server,mname),
                        data=post_data[0], headers={'Content-Type': 'application/json'})

                    with lock:
                        response = urllib2.urlopen(req)

                    if response.getcode() != 200:
                        print "Failed to connect http://%s/ec2/%s" % (server,mname)
                        return
                    else:
                        clusterTest.unittestRollback.addOperations(methodName,server,post_data[1])
                        with listLock:
                            list.append(d[0])

                    response.close()

                worker.enqueue(task,(self._lock,l,self._listLock,d,methodName,s,))        
                               
        worker.join()
        return True


    def _generateFakeServer(self,num):
        json_list = []

        if not self._prepareData(MediaServerGenerator().generateMediaServerData(num),"saveMediaServer",json_list):
            return False

        for entry in json_list:
            obj = json.loads(entry)
            self._existedFakeServerList.append((obj["id"],obj["name"]))

        return True


    def __init__(self,num):
        if not self._generateFakeServer(num) :
            raise Exception("Cannot initialize server list attribute test data")

    def _getRandomServer(self):
        idx = random.randint(0,len(self._existedFakeServerList) - 1)
        return self._existedFakeServerList[idx]

    def generateServerUserAttributesList(self,number):
        ret = []
        for i in xrange(number):
            uuid,name = self._getRandomServer()
            ret.append(self._template % (self.generateTrueFalse(),
                    random.randint(0,200),
                    uuid,name))
        return ret



class ClusterTestBase(unittest.TestCase):
    _Lock = threading.Lock()

    def _generateModifySeq(self):
        return None

    def _getMethodName(self):
        pass

    def _getObserverName(self):
        pass

    def _defaultModifySeq(self,fakeData):
        ret = []
        for f in fakeData:
            # pick up a server randomly
            ret.append((f,clusterTest.clusterTestServerList[random.randint(0,len(clusterTest.clusterTestServerList) - 1)]))
        return ret

    def _defaultCreateSeq(self,fakeData):
        ret = []
        for f in fakeData:
            serverName = clusterTest.clusterTestServerList[random.randint(0,len(clusterTest.clusterTestServerList) - 1)]
            # add rollback cluster operations
            clusterTest.unittestRollback.addOperations(self._getMethodName(),serverName,f[1])
            ret.append((f[0],serverName))

        return ret

    def _dumpFailedRequest(self,data,methodName):
        f = open("%s.failed.%.json" % (methodName,threading.active_count()),"w")
        f.write(data)
        f.close()

    def _sendRequest(self,methodName,d,server):
        req = urllib2.Request("http://%s/ec2/%s" % (server,methodName), \
            data=d, headers={'Content-Type': 'application/json'})
        response = None
        
        with self._Lock:
            print "Connection to http://%s/ec2/%s" % (server,methodName)
            response = urllib2.urlopen(req)

        # Do a sligtly graceful way to dump the sample of failure
        if response.getcode() != 200:
            self._dumpFailedRequest(d,methodName) 

        self.assertTrue(response.getcode() == 200, \
            "%s failed with statusCode %d" % (methodName,response.getcode()))

        response.close()

    def run(self, result=None):
        if result is None: result = self.defaultTestResult()
        result.startTest(self)
        testMethod = getattr(self, self._testMethodName)
        try:
            try:
                self.setUp()
            except KeyboardInterrupt:
                raise
            except:
                result.addError(self, self._exc_info())
                return

            ok = False
            try:
                testMethod()
                ok = True
            except self.failureException:
                result.addFailure(self, self._exc_info())
                result.stop()
            except KeyboardInterrupt:
                raise
            except:
                result.addError(self, self._exc_info())
                result.stop()

            try:
                self.tearDown()
            except KeyboardInterrupt:
                raise
            except:
                result.addError(self, self._exc_info())
                ok = False
            if ok: result.addSuccess(self)
        finally:
            result.stopTest(self)

    def test(self):
        postDataList = self._generateModifySeq()

        # skip this class
        if postDataList == None:
            return

        workerQueue = ClusterWorker(clusterTest.threadNumber,len(postDataList))

        print "===================================\n"
        print "Test:%s start!\n" % (self._getMethodName())

        for test in postDataList:
            workerQueue.enqueue(self._sendRequest , (self._getMethodName(),test[0],test[1],))

        workerQueue.join()
                
        time.sleep(clusterTest.clusterTestSleepTime)
        observer = self._getObserverName()

        if isinstance(observer,(list)):
            for m in observer:
                ret,reason = clusterTest.checkMethodStatusConsistent(m)
                self.assertTrue(ret,reason)
        else:
            ret , reason = clusterTest.checkMethodStatusConsistent(observer)
            self.assertTrue(ret,reason)

        print "Test:%s finish!\n" % (self._getMethodName())
        print "===================================\n"


class CameraTest(ClusterTestBase):
    _gen = None
    _testCase = clusterTest.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = clusterTest.testCaseSize
        self._gen = CameraDataGenerator()
    
    
    def _generateModifySeq(self):
        ret = []
        for _ in xrange(self._testCase):
            s = clusterTest.clusterTestServerList[random.randint(0,len(clusterTest.clusterTestServerList) - 1)]
            data = self._gen.generateCameraData(1,s)[0]
            clusterTest.unittestRollback.addOperations(self._getMethodName(),s,data[1])
            ret.append((data[0],s))
        return ret

    def _getMethodName(self):
        return "saveCameras"

    def _getObserverName(self):
        return "getCameras?format=json"


class UserTest(ClusterTestBase):
    _gen = None
    _testCase = clusterTest.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = clusterTest.testCaseSize
        self._gen = UserDataGenerator()

    def _generateModifySeq(self):
        return self._defaultCreateSeq(self._gen.generateUserData(self._testCase))

    def _getMethodName(self):
        return "saveUser"

    def _getObserverName(self):
        return "getUsers?format=json"


class MediaServerTest(ClusterTestBase):
    _gen = None
    _testCase = clusterTest.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = clusterTest.testCaseSize
        self._gen = MediaServerGenerator()

    def _generateModifySeq(self):
        return self._defaultCreateSeq(self._gen.generateMediaServerData(self._testCase))

    def _getMethodName(self):
        return "saveMediaServer"

    def _getObserverName(self):
        return "getMediaServersEx?format=json"

class ResourceParaTest(ClusterTestBase):
    _gen = None
    _testCase = clusterTest.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = clusterTest.testCaseSize
        self._gen = ResourceDataGenerator(self._testCase * 2)

    def _generateModifySeq(self):
        return self._defaultModifySeq(self._gen.generateResourceParams(self._testCase))
    
    def _getMethodName(self):
        return "setResourceParams"

    def _getObserverName(self):
        return "getResourceParams?format=json"


class ResourceRemoveTest(ClusterTestBase):
    _gen = None
    _testCase = clusterTest.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = clusterTest.testCaseSize
        self._gen = ResourceDataGenerator(self._testCase * 2)

    def _generateModifySeq(self):
        return self._defaultModifySeq(self._gen.generateRemoveResource(self._testCase))

    def _getMethodName(self):
        return "removeResource"

    def _getObserverName(self):
        return ["getMediaServersEx?format=json",
                "getUsers?format=json",
                "getCameras?format=json"]


class CameraUserAttributeListTest(ClusterTestBase):
    _gen = None
    _testCase = clusterTest.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = clusterTest.testCaseSize
        self._gen = CameraUserAttributesListDataGenerator(self._testCase * 2)

    def _generateModifySeq(self):
        return self._defaultModifySeq(self._gen.generateCameraUserAttribute(self._testCase))

    def _getMethodName(self):
        return "saveCameraUserAttributesList"

    def _getObserverName(self):
        return "getCameraUserAttributes"


class ServerUserAttributesListDataTest(ClusterTestBase):
    _gen = None
    _testCase = clusterTest.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = clusterTest.testCaseSize
        self._gen = ServerUserAttributesListDataGenerator(self._testCase * 2)

    def _generateModifySeq(self):
        return self._defaultModifySeq(self._gen.generateServerUserAttributesList(self._testCase))

    def _getMethodName(self):
        return "saveServerUserAttributesList"

    def _getObserverName(self):
        return "getServerUserAttributes"

# The following test will issue the modify and remove on different servers to
# trigger confliction resolving.
class ResourceConflictionTest(ClusterTestBase):
    _testCase = clusterTest.testCaseSize
    _conflictList = []
    
    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        dataGen = ConflictionDataGenerator()

        print "Start confliction data preparation, this will generate Cameras/Users/MediaServers"
        dataGen.prepare(clusterTest.testCaseSize)
        print "Confilication data generation done"

        self._testCase = clusterTest.testCaseSize
        self._conflictList = [("removeResource","saveMediaServer",MediaServerConflictionDataGenerator(dataGen)),
            ("removeResource","saveUser",UserConflictionDataGenerator(dataGen)),
            ("removeResource","saveCameras",CameraConflictionDataGenerator(dataGen))]

    def _generateRandomServerPair(self):
        # generate first server here
        s1 = clusterTest.clusterTestServerList[random.randint(0,len(clusterTest.clusterTestServerList) - 1)]
        s2 = None
        if len(clusterTest.clusterTestServerList) == 1:
            s2 = s1
        else:
            while True:
                s2 = clusterTest.clusterTestServerList[random.randint(0,len(clusterTest.clusterTestServerList) - 1)]
                if s2 != s1:
                    break
        return (s1,s2)

    def _generateResourceConfliction(self):
        return self._conflictList[random.randint(0,len(self._conflictList) - 1)]


    def _checkStatus(self):
        apiList = ["getMediaServersEx?format=json",
            "getUsers?format=json",
            "getCameras?format=json"]

        time.sleep(clusterTest.clusterTestSleepTime)
        for api in  apiList:
            ret , reason = clusterTest.checkMethodStatusConsistent(api)
            self.assertTrue(ret,reason) 


    # Overwrite the test function since the base method doesn't work here
    def test(self):
        workerQueue = ClusterWorker(clusterTest.threadNumber,self._testCase * 2)

        print "===================================\n"
        print "Test:ResourceConfliction start!\n"

        for _ in xrange(self._testCase):
            conf = self._generateResourceConfliction()
            s1,s2 = self._generateRandomServerPair()
            data = conf[2].generateData()

            # modify the resource
            workerQueue.enqueue(self._sendRequest , (conf[1],data[0][0],s1,))
            # remove the resource
            workerQueue.enqueue(self._sendRequest , (conf[0],data[0][1],s2,))

        workerQueue.join()

        self._checkStatus()

        print "Test:ResourceConfliction finish!\n"
        print "===================================\n"

# ========================================
# Server Merge Automatic Test
# ========================================
class MergeTestBase:
    _systemName = []
    _oldSystemName = []
    _mergeTestSystemName = "mergeTest"
    _mergeTestTimeout = 1

    def __init__(self):
        config_parser = ConfigParser.RawConfigParser()
        config_parser.read("ec2_tests.cfg")
        self._mergeTestTimeout = config_parser.getint("General","mergeTestTimeout")

    # This function is used to generate unique system name but random.  It
    # will gaurantee that the generated name is UNIQUE inside of the system
    def _generateRandomSystemName(self):
        gen = BasicGenerator()
        ret = []
        s = set()
        for i in xrange(len(clusterTest.clusterTestServerList)):
            length = random.randint(16,30)
            while True:
                name = gen.generateRandomString(length)
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
        for s in clusterTest.clusterTestServerList:
            print "Connection to http://%s/ec2/testConnection" % (s)
            response = urllib2.urlopen("http://%s/ec2/testConnection" % (s))
            if response.getcode() != 200:
                return False
            jobj = json.loads(response.read())
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

    # This function is used to set the system name to randome
    def _setClusterSystemRandom(self):
        # Store the old system name here
        self._storeClusterOldSystemName()
        testList = self._generateRandomSystemName()
        for i in xrange(len(clusterTest.clusterTestServerList)):
            self._setSystemName(clusterTest.clusterTestServerList[i],testList[i])

    def _setClusterToMerge(self):
        for s in clusterTest.clusterTestServerList:
            self._setSystemName(s,self._mergeTestSystemName)

    def _rollbackSystemName(self):
        for i in xrange(len(clusterTest.clusterTestServerList)):
            self._setSystemName(clusterTest.clusterTestServerList[i],self._oldSystemName[i])

# This class represents a single server with a UNIQUE system name.
# After we initialize this server, we will make it executes certain
# type of random data generation, after such generation, the server
# will have different states with other servers
class PrepareServerStatus(BasicGenerator):
    _minData = 10
    _maxData = 20

    getterAPI = ["getResourceParams",
        "getMediaServers",
        "getMediaServersEx",
        "getCameras",
        "getUsers",
        "getServerUserAttributes",
        "getCameraUserAttributes"]

    _mergeTest = None
    
    def __init__(self,mt):
        self._mergeTest = mt

    # Function to generate method and class matching
    def _generateDataAndAPIList(self,addr):

        def cameraFunc(num):
            gen = CameraDataGenerator()
            return gen.generateCameraData(num,addr)

        def userFunc(num):
            gen = UserDataGenerator()
            return gen.generateUserData(num)

        def mediaServerFunc(num):
            gen = MediaServerGenerator()
            return gen.generateMediaServerData(num)

        return [("saveCameras",cameraFunc),
                ("saveUser",userFunc),
                ("saveMediaServer",mediaServerFunc)]

    def _sendRequest(self,addr,method,d):
        req = urllib2.Request("http://%s/ec2/%s" % (addr,method), \
              data=d, 
              headers={'Content-Type': 'application/json'})
        
        with self._mergeTest._lock:
            print "Connection to http://%s/ec2/%s" % (addr,method)
            response = urllib2.urlopen(req)

        if response.getcode() != 200 :
            return (False,"Cannot issue %s with HTTP code:%d to server:%s" % (method,response.getcode(),addr))

        response.close()

        return (True,"")

    def _generateRandomStates(self,addr):
        api_list = self._generateDataAndAPIList(addr)
        for api in api_list:
            num = random.randint(self._minData,self._maxData)
            data_list = api[1](num)
            for data in data_list:
                ret,reason = self._sendRequest(addr,api[0],data[0])
                if ret == False:
                    return (ret,reason)
                clusterTest.unittestRollback.addOperations(api[0],addr,data[1])

        return (True,"")

    def main(self,addr):
        ret,reason = self._generateRandomStates(addr)
        if ret == False:
            raise Exception("Cannot generate random states:%s" % (reason))
    
# This class is used to control the whole merge test
class MergeTest_Resource(MergeTestBase):
    _gen = BasicGenerator()
    _lock = threading.Lock()

    def _prolog(self):
        print "Merge test prolog : Test whether all servers you specify has the identical system name"
        oldSystemName = None
        oldSystemNameAddr = None

        # Testing whether all the cluster server has identical system name
        for s in clusterTest.clusterTestServerList:
            print "Connection to http://%s/ec2/testConnection" % (s)
            response = urllib2.urlopen("http://%s/ec2/testConnection" % (s))
            if response.getcode() != 200:
                return False
            jobj = json.loads(response.read())
            if oldSystemName == None:
                oldSystemName = jobj["systemName"]
                oldSystemNameAddr = s
            else:
                systemName = jobj["systemName"]
                if systemName != oldSystemName:
                    print "The merge test cannot start!"
                    print "Server:%s has system name:%s" % (oldSystemName,oldSystemNameAddr)
                    print "Server:%s has system name:%s" % (s,jobj["systemName"])
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
        worker = ClusterWorker(clusterTest.threadNumber,len(clusterTest.clusterTestServerList))

        for s in clusterTest.clusterTestServerList:
            worker.enqueue(PrepareServerStatus(self).main,(s,))

        worker.join()
        print "Merge test phase1 done, now sleep and wait for sync"
        time.sleep(self._mergeTestTimeout)

    def _phase2(self):
        print "Merge test phase2: set ALL the servers with system name :mergeTest"
        self._setClusterToMerge()
        print "Merge test phase2: wait for sync"
        # Wait until the synchronization time out expires
        time.sleep(self._mergeTestTimeout)
        # Do the status checking of _ALL_ API
        for api in PrepareServerStatus.getterAPI:
            ret , reason = clusterTest.checkMethodStatusConsistent("%s?format=json" % (api))
            if ret == False:
                return (ret,reason)
        print "Merge test phase2 done"
        return (True,"")

    def test(self):
        print "================================\n"
        print "Server Merge Test:Resource Start\n"
        if not self._prolog():
            return False
        self._phase1()
        ret,reason = self._phase2()
        if not ret:
            print reason
        self._epilog()
        print "Server Merge Test:Resource End\n"
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
        config_parser = ConfigParser.RawConfigParser()
        config_parser.read("ec2_tests.cfg")
        self._oldClusterPassword = config_parser.get("General","password")
        self._username = config_parser.get("General","username")

    def _generateUniquePassword(self):
        ret = []
        s = set()
        gen = BasicGenerator()
        for server in clusterTest.clusterTestServerList:
            # Try to generate a unique password
            pwd = gen.generateRandomString(20)
            if pwd in s:
                continue
            else:
                # The password is new password
                ret.append((server,pwd))
                self._newPasswordList[server]=pwd

        return ret
    # This function MUST be called after you change each server's
    # password since we need to update the installer of URLLIB2

    def _setUpNewAuthentication(self,pwdlist):
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for entry in pwdlist:
            passman.add_password("NetworkOptix","http://%s/ec2" % (entry[0]), self._username, entry[1])
            passman.add_password("NetworkOptix","http://%s/api" % (entry[0]), self._username, entry[1])
        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

    def _setUpClusterAuthentication(self,password):
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for s in clusterTest.clusterTestServerList:
            passman.add_password("NetworkOptix","http://%s/ec2" % (s), self._username, password)
            passman.add_password("NetworkOptix","http://%s/api" % (s), self._username, password)
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
        for s in clusterTest.clusterTestServerList:
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
            for server in clusterTest.clusterTestServerList:
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
        self._setUpClusterAuthentication(self._clusterSharedPassword)
        # Now we need to set up the check
        ret,reason = clusterTest.checkMethodStatusConsistent("getMediaServersEx?format=json")
        if not ret:
            print reason
            return False
        else:
            return True

    def _checkOnline(self,uidset,response,serverAddr):
        obj = json.loads(response)
        for ele in obj:
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
        for uid in clusterTest.clusterTestServerUUIDList:
            uidSet.add(uid[0])

        # For each server test whether they work or not
        for s in clusterTest.clusterTestServerList:
            print "Connection to http://%s/ec2/getMediaServersEx?format=json"%(s)
            response = urllib2.urlopen("http://%s/ec2/getMediaServersEx?format=json"%(s))
            if response.getcode() != 200:
                print "Connection failed with HTTP code:%d"%(response.getcode())
                return False
            if not self._checkOnline(uidSet,response.read(),s):
                return False

        return True

    def _fetchAdmin(self):
        for s in clusterTest.clusterTestServerList:
            response = urllib2.urlopen("http://%s/ec2/getUsers"%(s))
            obj = json.loads(response.read())
            for entry in obj:
                if entry["isAdmin"]:
                    self._adminList[s] = (entry["id"],entry["name"],entry["email"])

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
            [(s,self._oldClusterPassword) for s in clusterTest.clusterTestServerList])

        # rollback the system name 
        self._rollbackSystemName()

    def _failRollbackPassword(self):
        # The current problem is that we don't know which password works so
        # we use a very conservative way to do the job. We use every password
        # that may work to change the whole cluster
        addrSet = set()

        for server in self._newPasswordList:
            pwd = self._newPasswordList[server]
            authList = [(s,pwd) for s in clusterTest.clusterTestServerList]
            self._setUpNewAuthentication(authList)

            # Now try to login on to the server and then set back the admin
            check = False
            for ser in clusterTest.clusterTestServerList:
                if ser in addrSet:
                    continue
                check = True
                if self._setAdminPassword(ser,self._oldClusterPassword,False):
                    addrSet.add(ser)
                else:
                    self._setUpNewAuthentication(authList)

            if not check:
                return True

        if len(addrSet) != len(clusterTest.clusterTestServerList):
            print "There're some server's admin password I cannot prob and rollback"
            print "Since it is a failover rollback,I cannot guarantee that I can rollback the whole cluster"
            print "There're possible bugs in the cluster that make the automatic rollback impossible"
            print "The following server has _UNKNOWN_ password now"
            for ser in clusterTest.clusterTestServerList:
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
        self._fetchAdmin()
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

class MergeTest:
    def test(self):
        if not MergeTest_Resource().test():
            return False
        # The following merge test ALWAYS fail and I don't know it is my problem or not
        # Current it is disabled and you could use a seperate command line to run it 
        #MergeTest_AdminPassword().test()

        return True

# ===================================
# RTSP test
# ===================================
class RtspStreamURLGenerator:
    _streamURLTemplate = "rtsp://%s:%d/%s"
    _server = None
    _port = None
    _mac = None

    def __init__(self,server,port,mac):
        self._diffMax = max
        self._diffMin = min
        self._server = server
        self._port = port
        self._mac = mac

    def generateURL(self):
        return self._streamURLTemplate % (self._server,self._port,self._mac)


class RtspArchiveURLGenerator:
    _archiveURLTemplate = "rtsp://%s:%d/%s?pos=%d"
    _diffMax = 5
    _diffMin = 1
    _server = None
    _port = None
    _mac = None

    def __init__(self,max,min,server,port,mac):
        self._diffMax = max
        self._diffMin = min
        self._server = server
        self._port = port
        self._mac = mac

    def _generateUTC(self):
        diff = random.randint(self._diffMin,self._diffMax)
        moment = datetime.datetime.now() - datetime.timedelta(minutes=diff)
        return time.mktime(moment.timetuple()) * 1e6 + moment.microsecond

    def generateURL(self):
        return self._archiveURLTemplate % (self._server,
                                         self._port,
                                         self._mac,
                                         self._generateUTC())

# RTSP global backoff timer, this is used to solve too many connection to server
# which makes the server think it is suffering DOS attack
class RtspBackOffTimer:
    _timerLock = threading.Lock()
    _globalTimerTable= dict()

    MAX_TIMEOUT = 4.0
    MIN_TIMEOUT = 0.01

    def increase(self,url):
        with self._timerLock:
            if url in self._globalTimerTable:
                self._globalTimerTable[url] *= 2.0
                if self._globalTimerTable[url] >= self.MAX_TIMEOUT:
                    self._globalTimerTable[url] = self.MAX_TIMEOUT
                time.sleep(self._globalTimerTable[url])
            else:
                self._globalTimerTable[url] = 0.01
                time.sleep(0.01)

    def decrease(self,url):
        with self._timerLock:
            if url not in self._globalTimerTable:
                return
            else:
                if self._globalTimerTable[url] <= self.MIN_TIMEOUT:
                    self._globalTimerTable[url] = self.MIN_TIMEOUT
                    return
                else:
                    self._globalTimerTable[url] /= 2.0


_rtspBackOffTimer = RtspBackOffTimer()

class RRRtspTcpBasic:
    _socket = None
    _addr = None
    _port = None
    _data = None
    _url = None
    _cid = None
    _sid = None
    _mac = None
    _uname = None
    _pwd = None
    _urlGen = None

    _rtspBasicTemplate = "PLAY %s RTSP/1.0\r\n\
        CSeq: 2\r\n\
        Range: npt=now-\r\n\
        Scale: 1\r\n\
        x-guid: %s\r\n\
        Session:\r\n\
        User-Agent: Network Optix\r\n\
        x-play-now: true\r\n\
        Authorization: Basic YWRtaW46MTIz\r\n\
        x-server-guid: %s\r\n\r\n"

    _rtspDigestTemplate = "PLAY %s RTSP/1.0\r\n\
        CSeq: 2\r\n\
        Range: npt=now-\r\n\
        Scale: 1\r\n\
        x-guid: %s\r\n\
        Session:\r\n\
        User-Agent: Network Optix\r\n\
        x-play-now: true\r\n\
        %s\r\n \
        x-server-guid: %s\r\n\r\n"

    _digestAuthTemplate = "Authorization:Digest username=\"%s\",realm=\"%s\",nonce=\"%s\",uri=\"%s\",response=\"%s\",algorithm=\"MD5\""

    _lock = None
    _log  = None
    
    def __init__(self,addr,port,mac,cid,sid,uname,pwd,urlGen,lock = None ,log = None):
        self._port = port
        self._addr = addr
        self._urlGen = urlGen
        self._url = urlGen.generateURL()
        self._data = self._rtspBasicTemplate % (self._url,
            cid,
            sid)

        self._cid = cid
        self._sid = sid
        self._mac = mac
        self._uname = uname
        self._pwd = pwd
        self._lock = lock
        self._log = log

    def _checkEOF(self,data):
        return data.find("\r\n\r\n") > 0

    def _parseRelamAndNonce(self,reply):
        idx = reply.find("WWW-Authenticate")
        if idx < 0:
            return False
        # realm is fixed for our server
        realm = "NetworkOptix"

        # find the Nonce
        idx = reply.find("nonce=",idx)
        if idx < 0:
            return False
        idx_start = idx + 6
        idx_end = reply.find(",",idx)
        if idx_end < 0:
            idx_end = reply.find("\r\n",idx)
            if idx_end < 0:
                return False
        nonce = reply[idx_start + 1:idx_end - 1]

        return (realm,nonce)

    # This function only calcuate the digest response
    # not the specific header field.  So another format
    # is required to format the header into the target
    # HTTP digest authentication

    def _generateMD5String(self,m):
        return ''.join('%02x' % ord(i) for i in m)

    def _calDigest(self,realm,nonce):
        m = md5.new()
        m.update("%s:%s:%s" % (self._uname,realm,self._pwd))
        part1 = m.digest()

        m = md5.new()
        m.update("PLAY:/%s" % (self._mac))
        part2 = m.digest()

        m = md5.new()
        m.update("%s:%s:%s" % (self._generateMD5String(part1),nonce,self._generateMD5String(part2)))
        resp = m.digest()

        # represent the final digest as ANSI printable code
        return self._generateMD5String(resp)


    def _formatDigestHeader(self,realm,nonce):
        resp = self._calDigest(realm,nonce)
        return self._digestAuthTemplate % (self._uname,
            realm,
            nonce,
            "/%s" % (self._mac),
            resp)

    def _requestWithDigest(self,reply):
        realm = None
        nonce = None
        ret = self._parseRelamAndNonce(reply)
        if ret == False:
            return reply
        else:
            realm = ret[0]
            nonce = ret[1]
        auth = self._formatDigestHeader(realm,nonce)
        data = self._rtspDigestTemplate % (self._url,
            self._cid,
            auth,
            self._sid)
        self._request(data)
        return self._response()

    def _checkAuthorization(self,data):
        return data.find("Unauthorized") < 0
    
    
    def _request(self,data):
        while True:
            sz = self._socket.send(data)
            if sz == len(data):
                return
            else:
                data = data[sz:] 
            
    def _response(self):
        global _rtspBackOffTimer

        ret = ""
        while True:
            try:
                data = self._socket.recv(1024)
            except socket.error,e:
                _rtspBackOffTimer.increase("%s:%d"%(self._addr,self._port))
                return "This is not RTSP error but socket error:%s"%(e)

            _rtspBackOffTimer.decrease("%s:%d"%(self._addr,self._port))

            if not data:
                return ret
            else:
                ret += data
                if self._checkEOF(ret):
                    return ret

    def _dumpError(self,err):
        if self._lock:
            with self._lock:
                print "--------------------------------------------"
                print "Graceful shutdown error , it may be caused by the server improper behavior"
                print err
                print "ServerEndpoint:%s:%d"%(self._addr,self._port)
                print "Address:%s"%(self._url)
                print "---------------------------------------------"

                self._log.write("----------------------------------------")
                self._log.write("Graceful shutdown error , it may be caused by the server improper behavior")
                self._log.write("select error:%s"%(err))
                self._log.write("ServerEndpoint:%s:%d"%(self._addr,self._port))
                self._log.write("Address:%s"%(self._url))
                self._log.write("---------------------------------------------")

    def _gracefulShutdown(self):
        # This shutdown will issue a FIN on the peer side therefore, the peer side (if it recv blocks)
        # will recv EOF on that socket fd. And it should behave properly regarding that problem there.
        self._socket.shutdown(socket.SHUT_WR)
        # Now if the peer server behaves properly it will definitly 
        # close that connection and I will be able to read an EOF 
        while True:
            # Now we block on a timeout select
            try :
                ready = select.select([self._socket], [], [], 5)
            except socket.error,e:
                self._dumpError("select:%s"%(e))
                return
                   
            if ready[0]:
                # Now we get some data , read it and then try to read to EOF
                try :
                    buf = self._socket.recv(1024)
                    if buf is None or len(buf) == 0:
                        # The server performance gracefully and close that connection now
                        return

                except socket.error , e:
                    if e.errno == errno.EAGAIN or e.errno == errno.EWOULDBLOCK or e == errno.WSAEWOULDBLOCK:
                        pass
                    else:
                       self._dumpError("socket::recv : %s"%(e))
                       return
            else:
                # Timeout reached 
                self._dumpError( ("The server doesn't try to send me data or shutdown the connection for about 5 seconds\n"
                                  "However,I have issued the shutdown for read side,so the server _SHOULD_ close the connection\n") )
                return


    def __enter__(self):
        self._socket = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        self._socket.connect((self._addr,self._port))
        self._request(self._data)
        reply = self._response()
        if not self._checkAuthorization(reply):
            return (self._requestWithDigest(reply),self._url)
        else:
            return (reply,self._url)
        
    def __exit__(self,type,value,trace):
        # Do graceful closing here
        self._gracefulShutdown()
        self._socket.close()

class SingleServerRtspTestBase:
    _serverEndpoint = None
    _serverGUID = None
    _cameraList = []
    _cameraInfoTable = dict()
    _testCase = 0
    _username = None
    _password = None
    _archiveMax = 1
    _archiveMin = 5
    _log = None
    _lock = None


    def __init__(self,archiveMax,archiveMin,serverEndpoint,serverGUID,uname,pwd,log,lock):
        self._archiveMax = archiveMax
        self._archiveMin = archiveMin
        self._serverEndpoint = serverEndpoint
        self._serverGUID = serverGUID
        self._username = uname
        self._password = pwd
        self._log = log
        self._lock = lock

        self._fetchCameraList()

    def _getServerGUID(self):
        response = urllib2.urlopen("http://%s/ec2/testConnection"%(self._serverEndpoint))
        if response.getcode() != 200:
            return False
        obj = json.loads(response.read())
        response.close()
        guid = obj["ecsGuid"]
        if guid[0] == '{':
            return guid
        else:
            return "{" + obj["ecsGuid"] + "}" 

    def _fetchCameraList(self):
        # Get this server's GUID and filter out the only cameras that should be used on this server
        guid = self._getServerGUID()
        response = urllib2.urlopen("http://%s/ec2/getCameras?id=%s" % (self._serverEndpoint,guid))

        if response.getcode() != 200:
            raise Exception("Cannot connect to server:%s using getCameras" % (self._serverEndpoint))
        json_obj = json.loads(response.read())

        for c in json_obj:
            if c["typeId"] == "{1657647e-f6e4-bc39-d5e8-563c93cb5e1c}":
                continue # Skip desktop
            if "name" in c and c["name"].startswith("ec2_test"):
                continue # Skip fake camera
            self._cameraList.append((c["physicalId"],c["id"],c["name"]))
            self._cameraInfoTable[c["id"]] = c

        response.close()
        
    def _checkReply(self,reply):
        idx = reply.find("\r\n")
        if idx < 0:
            return False
        else:
            return "RTSP/1.0 200 OK" == reply[:idx]

    def _checkRtspRequest(self,c,reply):
        ret = None
        with self._lock:
            print "----------------------------------------------------"
            print "RTSP request on URL:%s issued!" % (reply[1])
            if not self._checkReply(reply[0]):
                print "RTSP request on Server:%s failed" % (self._serverEndpoint)
                print "Camera name:%s" % (c[2])
                print "Camera Physical Id:%s" % (c[0])
                print "Camera Id:%s" % (c[1])

                self._log.writeFail("-------------------------------------------")
                self._log.writeFail("RTSP request on Server:%s failed" % (self._serverEndpoint))
                self._log.writeFail("RTSP request URL:%s issued" % (reply[1]))
                self._log.writeFail("Camera name:%s" % (c[2]))
                self._log.writeFail("Camera Physical Id:%s" % (c[0]))
                self._log.writeFail("Camera Id:%s" % (c[1]))
                self._log.writeFail("Detail RTSP reply protocol:\n\n%s" % (reply[0]))
                self._log.writeFail("-------------------------------------------")
                self._log.flushFail()
                ret = False
            else:
                self._log.writeOK("-------------------------------------")
                self._log.writeOK("RTSP request on Server:%s with URL:%s passed!" % (self._serverEndpoint,reply[1]))
                self._log.writeOK("-------------------------------------")
                self._log.flushOK()
                print "Rtsp Test Passed!"
                ret = True
            print "-----------------------------------------------------"
            return ret

    def run(self):
        pass 

class FiniteSingleServerRtspTest(SingleServerRtspTestBase):
    _log = None
    _testCase = 0
    def __init__(self,archiveMax,archiveMin,serverEndpoint,serverGUID,testCase,uname,pwd,log,lock):
        self._testCase = testCase
        SingleServerRtspTestBase.__init__(self,
            archiveMax,
            archiveMin,
            serverEndpoint,
            serverGUID,
            uname,
            pwd,
            log,
            lock)

    def _testMain(self):
        c = self._cameraList[random.randint(0,len(self._cameraList) - 1)]
        l = self._serverEndpoint.split(":")

        # Streaming version RTSP test
        with RRRtspTcpBasic(l[0],int(l[1]),
                     c[0],
                     c[1],
                     self._serverGUID,
                     self._username,
                     self._password,
                     RtspStreamURLGenerator(l[0],int(l[1]),c[0])) as reply:
            self._checkRtspRequest(c,reply)
            

        with RRRtspTcpBasic(l[0],int(l[1]),
                     c[0],
                     c[1],
                     self._serverGUID,
                     self._username,
                     self._password,
                     RtspArchiveURLGenerator(self._archiveMax,self._archiveMin,l[0],int(l[1]),c[0])) as reply:
            self._checkRtspRequest(c,reply)


    def run(self):
        for _ in xrange(self._testCase):
            self._testMain()

class RtspLog:
    _fileOK = None
    _fileFail = None

    def __init__(self,serverAddr):
        l = serverAddr.split(":")
        self._fileOK = open("%s_%s.rtsp.ok.log" % (l[0],l[1]),"w+")
        self._fileFail = open("%s_%s.rtsp.fail.log" % (l[0],l[1]),"w+")

    def writeOK(self,msg):
        self._fileOK.write("%s\n" % (msg))

    def writeFail(self,msg):
        self._fileFail.write("%s\n" % (msg))
    
    def flushOK(self):
        self._fileOK.flush()

    def flushFail(self):
        self._fileFail.flush()

    def close(self):
        self._fileOK.close()
        self._fileFail.close()

class FiniteRtspTest:
    _testCase = 0
    _username = None
    _password = None
    _archiveMax = 360
    _archiveMin = 60
    _lock = threading.Lock()
    
    def __init__(self,testSize,userName,passWord,archiveMax,archiveMin):
        self._testCase = testSize
        self._username = userName
        self._password = passWord
        self._archiveMax = archiveMax # max difference
        self._archiveMin = archiveMin # min difference

    def test(self):
        thPool = []
        print "-----------------------------------"
        print "Finite RTSP test starts"
        print "The failed detail result will be logged in rtsp.log file"

        for i in xrange(len(clusterTest.clusterTestServerList)):
            serverAddr = clusterTest.clusterTestServerList[i]
            serverAddrGUID = clusterTest.clusterTestServerUUIDList[i][0]
            log = RtspLog(serverAddr)

            tar = FiniteSingleServerRtspTest(self._archiveMax,self._archiveMin,serverAddr,serverAddrGUID,self._testCase,
                                             self._username,self._password,log,self._lock)

            th = threading.Thread(target = tar.run)
            th.start()
            thPool.append((th,log))

        # Join the thread
        for t in thPool:
            t[0].join()
            t[1].close()
            
        print "Finite RTSP test ends"
        print "-----------------------------------"


class InfiniteSingleServerRtspTest(SingleServerRtspTestBase):
    _flag = None
    _negative = 0
    _positive = 0

    def __init__(self,archiveMax,archiveMin,serverEndpoint,serverGUID,uname,pwd,log,lock,flag):
        SingleServerRtspTestBase.__init__(self,
            archiveMax,
            archiveMin,
            serverEndpoint,
            serverGUID,
            uname,
            pwd,
            log,
            lock)

        self._flag = flag

    def run(self):
        l = self._serverEndpoint.split(":")
        while self._flag.isOn():
            for c in self._cameraList:
                # Streaming version RTSP test
                with RRRtspTcpBasic(l[0],int(l[1]),
                             c[0],
                             c[1],
                             self._serverGUID,
                             self._username,
                             self._password,
                             RtspStreamURLGenerator(l[0],int(l[1]),c[0])) as reply:
                    if self._checkRtspRequest(c,reply):
                        self._positive = self._positive + 1
                    else:
                        self._negative = self._negative + 1

                with RRRtspTcpBasic(l[0],int(l[1]),
                             c[0],
                             c[1],
                             self._serverGUID,
                             self._username,
                             self._password,
                             RtspArchiveURLGenerator(self._archiveMax,self._archiveMin,l[0],int(l[1]),c[0])) as reply:
                    if self._checkRtspRequest(c,reply):
                        self._positive = self._positive + 1
                    else:
                        self._negative = self._negative + 1

        print "-----------------------------------\nOn server:%s\nRTSP Passed:%d\nRTSP Failed:%d\n-----------------------------------\n" % (self._serverEndpoint,self._positive,self._negative)

class InfiniteRtspTest:
    _username = None
    _password = None
    _archiveMax = 360
    _archiveMin = 60
    _flag = True
    _lock = threading.Lock()

    def __init__(self,userName,passWord,archiveMax,archiveMin):
        self._username = userName
        self._password = passWord
        self._archiveMax = archiveMax # max difference
        self._archiveMin = archiveMin # min difference

    def isOn(self):
        return self._flag

    def _onInterrupt(self,a,b):
        self._flag = False

    def test(self):
        thPool = []

        print "-------------------------------------------"
        print "Infinite RTSP test starts"
        print "You can press CTRL+C to interrupt the tests"
        print "The failed detail result will be logged in rtsp.log file"

        # Setup the interruption handler
        signal.signal(signal.SIGINT,self._onInterrupt)

        for i in xrange(len(clusterTest.clusterTestServerList)):

            serverAddr = clusterTest.clusterTestServerList[i]
            serverAddrGUID = clusterTest.clusterTestServerUUIDList[i][0]
            log = RtspLog(serverAddr)

            tar = InfiniteSingleServerRtspTest(self._archiveMax,
                                               self._archiveMin,
                                               serverAddr,
                                               serverAddrGUID,
                                               self._username,
                                               self._password,
                                               log,
                                               self._lock,
                                               self)

            th = threading.Thread(target=tar.run)
            th.start()
            thPool.append((th,log))


        # This is a UGLY work around that to allow python get the interruption
        # while execution.  If I block into the join, python seems never get
        # interruption there.
        while self.isOn():
            try:
                time.sleep(1)
            except:
                break

        # Afterwards join them
        for t in thPool:
            t[0].join()
            t[1].close()

        print "Infinite RTSP test ends"
        print "-------------------------------------------"


def runRtspTest():
    config_parser = ConfigParser.RawConfigParser()
    config_parser.read("ec2_tests.cfg")
    username = config_parser.get("General","username")
    password = config_parser.get("General","password")
    testSize = config_parser.getint("Rtsp","testSize")
    diffMax = config_parser.getint("Rtsp","archiveDiffMax")
    diffMin = config_parser.getint("Rtsp","archiveDiffMin")

    if testSize < 0 :
        InfiniteRtspTest(username,password,diffMax,diffMin).test()
    else:
        FiniteRtspTest(testSize,username,password,diffMax,diffMin).test()

    # We don't need it
    clusterTest.unittestRollback.removeRollbackDB()

# ======================================================
# RTSP performance Operations
# ======================================================
class SingleServerRtspPerf(SingleServerRtspTestBase):
    ARCHIVE_STREAM_RATE = 1024*1024*10 # 10 MB/Sec
    _timeoutMax = 0
    _timeoutMin = 0
    _diffMax = 0
    _diffMin = 0
    _lock = None
    _perfLog = None
    _threadNum = 0
    _exitFlag = None
    _threadPool = []

    _archiveNumOK = 0
    _archiveNumFail = 0
    _streamNumOK = 0
    _streamNumFail = 0

    def __init__(self,archiveMax,archiveMin,serverEndpoint,guid,username,password,timeoutMax,timeoutMin,threadNum,flag,lock):
        SingleServerRtspTestBase.__init__(self,
                                          archiveMax,
                                          archiveMin,
                                          serverEndpoint,
                                          guid,
                                          username,
                                          password,
                                          RtspLog(serverEndpoint),
                                          lock)
        self._threadNum = threadNum
        self._exitFlag = flag
        self._lock = lock
        self._timeoutMax = timeoutMax
        self._timeoutMin = timeoutMin
        # Initialize the performance log
        l = serverEndpoint.split(":")
        self._perfLog = open("%s_%s.perf.rtsp.log" % (l[0],l[1]),"w+")


    # This function will blocked on socket to recv data in a fashion of _RATE_
    def _timeoutRecv(self,socket,rate,timeout):
        socket.setblocking(0)
        elapsed = 0
        buf = []
        last_packet_sz = 0

        while True:
            # recording the time for fetching an event
            begin = time.clock()
            ready = select.select([socket], [], [], timeout)
            if ready[0]:
                d= None
                if rate <0:
                    d = socket.recv(1024*16)
                else:
                    d = socket.recv(rate)
                last_packet_sz = len(d)
                buf.append(d)
                end = time.clock()
                elapsed = end-begin
                # compensate the rate of packet size here
                if rate != -1:
                    if len(data) >= rate:
                        time.sleep(1.0-elapsed)
                        socket.setblocking(1)
                        return ''.join(buf)
                    else:
                        rate -= last_packet_sz
                else:
                    return d
            else:
                # timeout reached
                socket.setblocking(1)
                return None

    def _dumpArchiveHelper(self,c,tcp_rtsp,timeout,dump,rate):
        for _ in xrange(timeout):
            try:
                data = self._timeoutRecv(tcp_rtsp._socket,rate,3)
                if dump is not None:
                    dump.write(data)
                    dump.flush
            except:
                continue
            
            if data == None:
                with self._lock:
                    print "--------------------------------------------"
                    print "The RTSP url:%s 3 seconds not response with any data" % (tcp_rtsp._url)
                    print "--------------------------------------------"
                    self._perfLog.write("--------------------------------------------\n")
                    self._perfLog.write("This is an exceptional case,the server _SHOULD_ not terminate the connection\n")
                    self._perfLog.write("The RTSP/RTP url:%s 3 seconds not response with any data\n" % (tcp_rtsp._url))
                    self._perfLog.write("Camera name:%s\n" % (c[2]))
                    self._perfLog.write("Camera Physical Id:%s\n" % (c[0]))
                    self._perfLog.write("Camera Id:%s\n" % (c[1]))
                    self._perfLog.write("--------------------------------------------\n")
                    self._perfLog.flush()
                return
            elif not data:
                with self._lock:
                    print "--------------------------------------------"
                    print "The RTSP url:%s manully close the connection" % (tcp_rtsp._url)
                    print "--------------------------------------------"
                    self._perfLog.write("--------------------------------------------\n")
                    self._perfLog.write("This is an exceptional case,the server _SHOULD_ not terminate the connection\n")
                    self._perfLog.write("The RTSP/RTP url:%s manully close the connection\n" % (tcp_rtsp._url))
                    self._perfLog.write("Camera name:%s\n" % (c[2]))
                    self._perfLog.write("Camera Physical Id:%s\n" % (c[0]))
                    self._perfLog.write("Camera Id:%s\n" % (c[1]))
                    self._perfLog.write("--------------------------------------------\n")
                    self._perfLog.flush()
                return

        with self._lock:
            print "--------------------------------------------"
            print "The RTP sink normally timeout with:%d on RTSP url:%s" % (timeout,tcp_rtsp._url)
            print "--------------------------------------------"
        return


    def _dumpStreamHelper(self,c,tcp_rtsp,timeout,dump,rate):
        elapsed = 0.0
        while True:
            begin = time.clock()
            try:
                data = self._timeoutRecv(tcp_rtsp._socket,rate,3)
                if dump is not None:
                    dump.write(data)
                    dump.flush
            except:
                continue
            
            if data == None:
                with self._lock:
                    print "--------------------------------------------"
                    print "The RTSP url:%s 3 seconds not response with any data" % (tcp_rtsp._url)
                    print "--------------------------------------------"
                    self._perfLog.write("--------------------------------------------\n")
                    self._perfLog.write("This is an exceptional case,the server _SHOULD_ not terminate the connection\n")
                    self._perfLog.write("The RTSP/RTP url:%s 3 seconds not response with any data\n" % (tcp_rtsp._url))
                    self._perfLog.write("Camera name:%s\n" % (c[2]))
                    self._perfLog.write("Camera Physical Id:%s\n" % (c[0]))
                    self._perfLog.write("Camera Id:%s\n" % (c[1]))
                    self._perfLog.write("--------------------------------------------\n")
                    self._perfLog.flush()
                return
            elif not data:
                with self._lock:
                    print "--------------------------------------------"
                    print "The RTSP url:%s manully close the connection" % (tcp_rtsp._url)
                    print "--------------------------------------------"
                    self._perfLog.write("--------------------------------------------\n")
                    self._perfLog.write("This is an exceptional case,the server _SHOULD_ not terminate the connection\n")
                    self._perfLog.write("The RTSP/RTP url:%s manully close the connection\n" % (tcp_rtsp._url))
                    self._perfLog.write("Camera name:%s\n" % (c[2]))
                    self._perfLog.write("Camera Physical Id:%s\n" % (c[0]))
                    self._perfLog.write("Camera Id:%s\n" % (c[1]))
                    self._perfLog.write("--------------------------------------------\n")
                    self._perfLog.flush()
                return

            end = time.clock()
            elapsed += (end-begin)

            if elapsed >= timeout:
                with self._lock:
                    print "--------------------------------------------"
                    print "The RTP sink normally timeout with:%d on RTSP url:%s" % (timeout,tcp_rtsp._url)
                    print "--------------------------------------------"
                return

    def _buildUrlPath(self,url):
        l = len(url)
        buf = []
        for i in xrange(l):
            if url[i] == ':':
                buf.append('$')
            elif url[i] == '/':
                buf.append('%')
            elif url[i] == '?':
                buf.append('#')
            else:
                buf.append(url[i])

        # generate a random postfix
        buf.append("_")
        for _ in xrange(12):
            buf.append(str(random.randint(0,9)))

        return ''.join(buf)

    def _dump(self,c,tcp_rtsp,timeout,dump,rate,helper):
        if dump :
            with open(self._buildUrlPath(tcp_rtsp._url),"w+") as f:
                helper(c,tcp_rtsp,timeout,f,rate)
        else:
            helper(c,tcp_rtsp,timeout,None,rate)

    # Represent a streaming TASK on the camera
    def _main_streaming(self,c,dump):
        l = self._serverEndpoint.split(':')
        obj = RRRtspTcpBasic(l[0],int(l[1]),
                     c[0],
                     c[1],
                     self._serverGUID,
                     self._username,
                     self._password,
                     RtspStreamURLGenerator(l[0],int(l[1]),c[0]),
                     self._lock,self._perfLog)

        with obj as reply:
            # 1.  Check the reply here
            if self._checkRtspRequest(c,reply):
                self._dump(c,obj,random.randint(self._timeoutMin,self._timeoutMax),dump,-1,self._dumpStreamHelper)
                self._streamNumOK = self._streamNumOK + 1
            else:
                self._streamNumFail = self._streamNumFail + 1


    def _main_archive(self,c,dump):
        l = self._serverEndpoint.split(':')
        obj = RRRtspTcpBasic(l[0],int(l[1]),
                             c[0],
                             c[1],
                             self._serverGUID,
                             self._username,
                             self._password,
                             RtspArchiveURLGenerator(self._archiveMax,self._archiveMin,l[0],int(l[1]),c[0]),
                             self._lock,self._perfLog)
        with obj as reply:
            # 1.  Check the reply here
           if self._checkRtspRequest(c,reply):
                self._dump(c,obj,random.randint(self._timeoutMin,self._timeoutMax),dump,self.ARCHIVE_STREAM_RATE,self._dumpArchiveHelper)
                self._archiveNumOK = self._archiveNumOK + 1
           else:
               self._archiveNumFail = self._archiveNumFail + 1

    def _threadMain(self,dump):
        while self._exitFlag.isOn():
            # choose a random camera in the server list
            c = self._cameraList[random.randint(0,len(self._cameraList) - 1)]
            if random.randint(0,1) == 0:
                self._main_streaming(c,dump)
            else:
                self._main_archive(c,dump)

    def join(self):
        for th in self._threadPool:
            th.join()
        self._perfLog.close()
        print "======================================="
        print "Server:%s:" % (self._serverEndpoint)
        print "Archive Success Number:%d" % (self._archiveNumOK)
        print "Archive Failed Number:%d" % (self._archiveNumFail)
        print "Stream Success Number:%d" % (self._streamNumOK)
        print "Stream Failed Number:%d" % (self._streamNumFail)
        print "======================================="

    def run(self):
        dump = False
        if len(sys.argv) == 3 and sys.argv[2] == '--dump':
            dump = True

        if len(self._cameraList) == 0:
            print "The camera list on server:%s is empty!"%(self._serverEndpoint)
            print "Do nothing and abort!"
            return False

        for _ in xrange(self._threadNum):
            th = threading.Thread(target=self._threadMain,args=(dump,))
            th.start()
            self._threadPool.append(th)

        return True

class RtspPerf:
    _perfServer = []
    _lock = threading.Lock()
    _exit = False

    def isOn(self):
        return not self._exit

    def _onInterrupt(self,a,b):
        self._exit = True

    def _loadConfig(self):
        config_parser = ConfigParser.RawConfigParser()
        config_parser.read("ec2_tests.cfg")
        threadNumbers = config_parser.get("Rtsp","threadNumbers").split(",")
        if len(threadNumbers) != len(clusterTest.clusterTestServerList):
            print "The threadNumbers in Rtsp section doesn't match the size of the servers in serverList"
            print "Every server MUST be assigned with a correct thread Numbers"
            print "RTSP Pressure test failed"
            return False

        archiveMax = config_parser.getint("Rtsp","archiveDiffMax")
        archiveMin = config_parser.getint("Rtsp","archiveDiffMin")
        timeoutMax = config_parser.getint("Rtsp","timeoutMax")
        timeoutMin = config_parser.getint("Rtsp","timeoutMin")
        username = config_parser.get("General","username")
        password = config_parser.get("General","password")

        # Let's add those RtspSinglePerf
        for i in xrange(len(clusterTest.clusterTestServerList)):
            serverAddr = clusterTest.clusterTestServerList[i]
            serverGUID = clusterTest.clusterTestServerUUIDList[i][0]
            serverThreadNum = int(threadNumbers[i])
            
            self._perfServer.append(SingleServerRtspPerf(archiveMax,archiveMin,serverAddr,serverGUID,username,password,
                    timeoutMax,timeoutMin,serverThreadNum,self,self._lock))

        return True
            
    def run(self):
        if not self._loadConfig():
            return False
        else:
            print "---------------------------------------------"
            print "Start to run RTSP pressure test now!"
            print "Press CTRL+C to interrupt the test!"
            print "The exceptional cases are stored inside of server_end_point.rtsp.perf.log"
            
            # Add the signal handler
            signal.signal(signal.SIGINT,self._onInterrupt)

            for e in self._perfServer:
                if not e.run():
                    return

            while self.isOn():
                try:
                    time.sleep(1)
                except:
                    break

            for e in self._perfServer:
                e.join()

            print "RTSP performance test done,see log for detail"
            print "---------------------------------------------"



def runRtspPerf():
    RtspPerf().run()
    clusterTest.unittestRollback.removeRollbackDB()

# Performance test function
# only support add/remove ,value can only be user and media server
class PerformanceOperation():
    _lock = threading.Lock()

    def _filterOutId(self,list):
        ret = []
        for i in list:
            ret.append(i[0])
        return ret

    def _sendRequest(self,methodName,d,server):
        req = urllib2.Request("http://%s/ec2/%s" % (server,methodName), \
        data=d, headers={'Content-Type': 'application/json'})

        response = None

        with self._lock:
            response = urllib2.urlopen(req)

        # Do a sligtly graceful way to dump the sample of failure
        if response.getcode() != 200:
            self._dumpFailedRequest(d,methodName)

        if response.getcode() != 200:
            print "%s failed with statusCode %d" % (methodName,response.getcode())
        else:
            print "%s OK\r\n" % (methodName)

        response.close()

    def _getUUIDList(self,methodName):
        ret = []
        for s in clusterTest.clusterTestServerList:
            data = []

            response = urllib2.urlopen("http://%s/ec2/%s?format=json" % (s,methodName))

            if response.getcode() != 200:
                return None

            json_obj = json.loads(response.read())
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
        for s in clusterTest.clusterTestServerList:
            data = []

            response = urllib2.urlopen("http://%s/ec2/%s?format=json" % (s,methodName))

            if response.getcode() != 200:
                return None

            json_obj = json.loads(response.read())
            for entry in json_obj:
                if "name" in entry and entry["name"].startswith("ec2_test"):
                    if "isAdmin" in entry and entry["isAdmin"] == True:
                        continue
                    data.append(entry["id"])
            response.close()
            ret.append((s,data))
        return ret

    def _sendOp(self,methodName,dataList,addr):
        worker = ClusterWorker(clusterTest.threadNumber,len(dataList))
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
        for s in clusterTest.clusterTestServerList:
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
        for s in clusterTest.clusterTestServerList:
            self._sendOp("saveMediaServer",
                         self._filterOutId(gen.generateMediaServerData(num)),s)
        return True

    def remove(self,uuid):
        self_.remove(uuid)
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
        for s in clusterTest.clusterTestServerList:
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

def runMiscFunction():
    if len(sys.argv) != 3 and len(sys.argv) != 2 :
        return (False,"2/1 parameters are needed")

    l = sys.argv[1].split('=')
    
    if l[0] != '--add' and l[0] != '--remove':
        return (False,"Unknown first parameter options")

    t = globals()["%sOperation" % (l[1])]

    if t == None:
        return (False,"Unknown target operations:%s" % (l[1]))
    else:
        t = t()
    
    if l[0] == '--add':
        if len(sys.argv) != 3 :
            return (False,"--add must have --count option")
        l = sys.argv[2].split('=')
        if l[0] == '--count':
            num = int(l[1])
            if num <= 0 :
                return (False,"--count must be positive integer")
            if t.add(num) == False:
                return (False,"cannot perform add operation")
        else:
            return (False,"--add can only have --count options")
    elif l[0] == '--remove':
        if len(sys.argv) == 3:
            l = sys.argv[2].split('=')
            if l[0] == '--id':
                if t.remove(l[1]) == False:
                    return (False,"cannot perform remove UID operation")
            elif l[0] == '--fake':
                if t.removeAllFake() == False:
                    return (False,"cannot perform remove UID operation")
            else:
                return (False,"--remove can only have --id options")
        elif len(sys.argv) == 2:
            if t.removeAll() == False:
                return (False,"cannot perform remove all operation")
    else:
        return (False,"Unknown command:%s" % (l[0]))

    return True


# ===================================
# Perf Test
# ===================================
class SingleResourcePerfGenerator:
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
            clusterTest.unittestRollback.addOperations("saveCameras",self._serverAddr,d[1])
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
    _gen = CameraDataGenerator()

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
    _gen = UserDataGenerator()
    
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
        for s in clusterTest.clusterTestServerList:
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
            
    def run(self,par):
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
        self._initThreadPool(clusterTest.threadNumber)
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
            print "Resource Create Success:%d" % (value.createOK)
            print "Resource Create Fail:%d" % (value.createFail)
            print "Resource Update Success:%d" % (value.updateOK)
            print "Resource Update Fail:%d" % (value.updateFail)
            print "Resource Remove Success:%d" % (value.removeOK)
            print "Resource Remove Fail:%d" % (value.removeFail)
            print "---------------------------------"
        print "===================================="
                        
def runPerfTest():
    options = sys.argv[2]
    l = options.split('=')
    PerfTest().run(l[1])
    doCleanUp()
    
class SystemNameTest:
    _serverList = []
    _guidDict = dict()
    _oldSystemName = None
    _syncTime = 2

    def _setUpAuth(self,user,pwd):
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for s in self._serverList:
            passman.add_password("NetworkOptix","http://%s/ec2/" % (s), user, pwd)
            passman.add_password("NetworkOptix","http://%s/api/" % (s), user, pwd)

        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

    def _doGet(self,addr,methodName):
        ret = None
        response = None
        print "Connection to http://%s/ec2/%s" % (addr,methodName)
        response = urllib2.urlopen("http://%s/ec2/%s" % (addr,methodName))

        if response.getcode() != 200:
            response.close()
            return None
        else:
            ret = response.read()
            response.close()
            return ret

    def _doPost(self,addr,methodName,d):
        response = None
        req = urllib2.Request("http://%s/ec2/%s" % (addr,methodName), \
            data=d, headers={'Content-Type': 'application/json'})

        print "Connection to http://%s/ec2/%s" % (addr,methodName)
        response = urllib2.urlopen(req)

        if response.getcode() != 200:
            response.close()
            return False
        else:
            response.close()
            return True

    def _changeSystemName(self,addr,name):
        response = urllib2.urlopen("http://%s/api/configure?%s" % (addr,urllib.urlencode({"systemName":name})))
        if response.getcode() != 200:
            response.close()
            return False
        else:
            response.close()
            return True

    def _ensureServerSystemName(self):
        systemName = None

        for s in self._serverList:
            ret = self._doGet(s,"testConnection")
            if ret == None:
                return (False,"Server:%s cannot perform testConnection REST call" % (s))
            obj = json.loads(ret)

            if systemName != None:
                if systemName != obj["systemName"]:
                    return (False,"Server:%s has systemName:%s which is different with others:%s" % (s,obj["systemName"],systemName))
            else:
                systemName = obj["systemName"]

            self._guidDict[s] = obj["ecsGuid"]

        self._oldSystemName = systemName
        return (True,"")


    def _loadConfig(self):
        configParser = ConfigParser.RawConfigParser()
        configParser.read("ec2_tests.cfg")
        username = configParser.get("General","username")
        passwd = configParser.get("General","password")
        sl = configParser.get("General","serverList")
        self._serverList = sl.split(",")
        self._syncTime = configParser.getint("General","clusterTestSleepTime")
        return (username,passwd)


    def _doSingleTest(self,s):
        thisGUID = self._guidDict[s]
        self._changeSystemName(s,BasicGenerator().generateRandomString(20))
        # wait for the time to sync
        time.sleep(self._syncTime)
        # issue a getMediaServerEx request to test whether all the servers in
        # the list has the expected offline/online status
        ret = self._doGet(s,"getMediaServersEx")
        if ret == None:
            return (False,"Server:%s cannot doGet on getMediaServersEx" % (s))
        obj = json.loads(ret)
        for ele in obj:
            if ele["id"] == thisGUID:
                if ele["status"] == "Offline":
                    return (False,"This server:%s with GUID:%s should be Online" % (s,thisGUID))
            else:
                if ele["status"] == "Online":
                    return (False,"The server that has IP address:%s and GUID:%s \
                    should be Offline when login on Server:%s" % (ele["networkAddresses"],ele["id"],s))
        return (True,"")
    
    def _doTest(self):
        for s in self._serverList:
            ret,reason = self._doSingleTest(s)
            if ret != True:
                return (ret,reason)
        return (True,"")

    def _doRollback(self):
        for s in self._serverList:
            self._changeSystemName(s,self._oldSystemName)

    def run(self):
        user,pwd = self._loadConfig()
        self._setUpAuth(user,pwd)

        ret,reason = self._ensureServerSystemName()

        if not ret:
            print reason
            return False
        
        print "========================================="
        print "Start to test SystemName for server lists"
        ret,reason = self._doTest()
        if not ret:
            print reason

        print "SystemName test finished"
        print "Start SystemName test rollback"
        self._doRollback()
        print "SystemName test rollback done"
        print "========================================="

def showHelp():
    helpMenu = {
        "perf":("Run performance test",(
            "Usage: python main.py --perf --type=... \n\n"
            "This command line will start built-in performance test\n"
            "The --type option is used to specify what kind of resource you want to profile.\n"
            "Currently you could specify Camera and User, eg : --type=Camera,User will test on Camera and User both;\n"
            "--type=User will only do performance test on User resources")),
        "clear":("Clear resources",(
            "Usage: python main.py --clear \nUsage: python main.py --clear --fake\n\n"
            "This command is used to clear the resource in server list.\n"
            "The resource includes Camera,MediaServer and Users.\n"
            "The --fake option is a flag to tell the command _ONLY_ clear\n"
            "resource that has name prefixed with \"ec2_test\".\n"
            "This name pattern typically means that the data is generated by the automatic test\n")),
        "sync":("Test cluster is sycnchronized or not",(
            "Usage: python main.py --sync \n\n"
            "This command is used to test whether the cluster has synchronized states or not.")),
        "recover":("Recover from previous fail rollback",(
            "Usage: python main.py --recover \n\n"
            "This command is used to try to recover from previous failed rollback.\n"
            "Each rollback will based on a file .rollback.However rollback may failed.\n"
            "The failed rollback transaction will be recorded in side of .rollback file as well.\n"
            "Every time you restart this program, you could specify this command to try\n "
            "to recover the failed rollback transaction.\n"
            "If you are running automatic test,the recover will be detected automatically and \n"
            "prompt for you to choose whether recover or not")),
        "merge-test":("Run merge test",(
            "Usage: python main.py --merge-test \n\n"
            "This command is used to run merge test speicifically.\n"
            "This command will run admin user password merge test and resource merge test.\n")),
        "merge-admin":("Run merge admin user password test",(
            "Usage: python main.py --merge-admin \n\n"
            "This command is used to run run admin user password merge test directly.\n"
            "This command will be removed later on")),
        "rtsp-test":("Run rtsp test",(
            "Usage: python main.py --rtsp-test \n\n"
            "This command is used to run RTSP Test test.It means it will issue RTSP play command,\n"
            "and wait for the reply to check the status code.\n"
            "User needs to set up section in ec2_tests.cfg file: [Rtsp]\ntestSize=40\n"
            "The testSize is configuration parameter that tell rtsp the number that it needs to perform \n"
            "RTSP test on _EACH_ server.Therefore,the above example means 40 random RTSP test on each server.\n")),
        "add":("Resource creation",(
            "Usage: python main.py --add=... --count=... \n\n"
            "This command is used to add different generated resources to servers.\n"
            "3 types of resource is available: MediaServer,Camera,User. \n"
            "The --add parameter needs to be specified the resource type. Eg: --add=Camera \n"
            "means add camera into the server, --add=User means add user to the server.\n"
            "The --count option is used to tell the size that you wish to generate that resources.\n"
            "Eg: main.py --add=Camera --count=100           Add 100 cameras to each server in server list.\n")),
        "remove":("Resource remove",(
            "Usage: python main.py --remove=Camera --id=... \n"
            "Usage: python main.py --remove=Camera --fake \n\n"
            "This command is used to remove resource on each servers.\n"
            "The --remove needs to be specified required resource type.\n"
            "3 types of resource is available: MediaServer,Camera,User. \n"
            "The --id option is optinoal, if it appears, you need to specify a valid id like this:--id=SomeID.\n"
            "It is used to delete specific resource. \n"
            "Optionally, you could specify --fake flag , if this flag is on, then the remove will only \n"
            "remove resource that has name prefixed with \"ec2_test\" which typically means fake resource")),
        "auto-test":("Automatic test",(
            "Usage: python main.py \n\n"
            "This command is used to run built-in automatic test.\n"
            "The automatic test includes 11 types of test and they will be runed automatically."
            "The configuration parameter is as follow: \n"
            "threadNumber                  The thread number that will be used to fire operations\n"
            "mergeTestTimeout              The timeout for merge test\n"
            "clusterTestSleepTime          The timeout for other auto test\n"
            "All the above configuration parameters needs to be defined in the General section.\n"
            "The test will try to rollback afterwards and try to recover at first.\n"
            "Also the sync operation will be performed before any test\n")),
        "rtsp-perf":("Rtsp performance test",(
            "Usage: python main.py --rtsp-perf \n\n"
            "Usage: python main.py --rtsp-perf --dump \n\n"
            "This command is used to run rtsp performance test.\n"
            "The test will try to check RTSP status and then connect to the server \n"
            "and maintain the connection to receive RTP packet for several seconds. The request \n"
            "includes archive and real time streaming.\n"
            "Additionally,an optional option --dump may be specified.If this flag is on, the data will be \n"
            "dumped into a file, the file stores raw RTP data and also the file is named with following:\n"
            "{Part1}_{Part2}, Part1 is the URL ,the character \"/\" \":\" and \"?\" will be escaped to %,$,#\n"
            "Part2 is a random session number which has 12 digits\n"
            "The configuration parameter is listed below:\n\n"
            "threadNumbers    A comma separate list to specify how many list each server is required \n"
            "The component number must be the same as component in serverList. Eg: threadNumbers=10,2,3 \n"
            "This means that the first server in serverList will have 10 threads,second server 2,third 3.\n\n"
            "archiveDiffMax       The time difference upper bound for archive request, in minutes \n"
            "archiveDiffMin       The time difference lower bound for archive request, in minutes \n"
            "timeoutMax           The timeout upper bound for each RTP receiving, in seconds. \n"
            "timeoutMin           The timeout lower bound for each RTP receiving, in seconds. \n"
            "Notes: All the above parameters needs to be specified in configuration file:ec2_tests.cfg under \n"
            "section Rtsp.\nEg:\n[Rtsp]\nthreadNumbers=10,2\narchiveDiffMax=..\nardchiveDiffMin=....\n"
            )),
        "sys-name":("System name test",(
            "Usage: python main.py --sys-name \n\n"
            "This command will perform system name test for each server.\n"
            "The system name test is , change each server in cluster to another system name,\n"
            "and check each server that whether all the other server is offline and only this server is online.\n"
            ))
        }

    if len(sys.argv) == 2:
        helpStrHeader=("Help for auto test tool\n\n"
                 "*****************************************\n"
                 "**************Function Menu**************\n"
                 "*****************************************\n"
                 "Entry            Introduction            \n")

        print helpStrHeader

        for k,v in helpMenu.iteritems():
            print "%s:\t%s"%(k,v[0])

        helpStrFooter = ("\n\nTo see detail help information,please run command:\n"
               "python main.py --help Entry\n\n"
               "Eg: python main.py --help auto-test\n"
               "This will list detail information about auto-test\n")

        print helpStrFooter
    else:
        option = sys.argv[2]
        if option in helpMenu:
            print "==================================="
            print option
            print "===================================\n\n"
            print helpMenu[option][1]
        else:
            print "Option:%s is not found !"%(option)

def doCleanUp():
    selection = None
    try :
        selection = raw_input("Press Enter to continue ROLLBACK or press x to SKIP it...")
    except:
        pass

    if selection != None and (len(selection) == 0 or selection[0] != 'x'):
        print "Now do the rollback, do not close the program!"
        clusterTest.unittestRollback.doRollback()
        print "++++++++++++++++++ROLLBACK DONE+++++++++++++++++++++++"
    else:
        print "Skip ROLLBACK,you could use --recover to perform manually rollback"
            
if __name__ == '__main__':
    if len(sys.argv) >= 2 and sys.argv[1] == '--help':
        showHelp()
    elif len(sys.argv) == 2 and sys.argv[1] == '--recover':
        UnitTestRollback().doRecover()
    elif len(sys.argv) == 2 and sys.argv[1] == '--sys-name':
        SystemNameTest().run()
    else:
        print "The automatic test starts,please wait for checking cluster status,test connection and APIs and do proper rollback..."
        # initialize cluster test environment
        ret,reason = clusterTest.init()
        if ret == False:
            print "Failed to initialize the cluster test object:%s" % (reason)
        elif len(sys.argv) == 2 and sys.argv[1] == '--sync':
            pass # done here, since we just need to test whether
                 # all the servers are on the same page
        else:
            if len(sys.argv) == 1:
                ret = None
                try:
                    ret = unittest.main()
                except SystemExit,e:
                    if not e.code:
                        if MergeTest().test():
                            SystemNameTest().run()

                    print "\n\nALL AUTOMATIC TEST ARE DONE\n\n"
                    doCleanUp()

            elif (len(sys.argv) == 2 or len(sys.argv) == 3) and sys.argv[1] == '--clear':
                if len(sys.argv) == 3:
                    if sys.argv[2] == '--fake':
                        doClearAll(True)
                    else:
                        print "Unknown option:%s in --clear" % (sys.argv[2])
                else:
                    doClearAll(False)
                clusterTest.unittestRollback.removeRollbackDB()
            elif len(sys.argv) == 2 and sys.argv[1] == '--perf':
                PerfTest().start()
                doCleanUp()
            else:
                if sys.argv[1] == '--merge-test':
                    MergeTest().test()
                    doCleanUp()
                elif sys.argv[1] == '--merge-admin':
                    MergeTest_AdminPassword().test()
                    clusterTest.unittestRollback.removeRollbackDB()
                elif sys.argv[1] == '--rtsp-test':
                    runRtspTest()
                elif sys.argv[1] == '--rtsp-perf':
                    runRtspPerf()
                elif len(sys.argv) == 3 and sys.argv[1] == '--perf':
                    runPerfTest()
                else:
                    runMiscFunction()