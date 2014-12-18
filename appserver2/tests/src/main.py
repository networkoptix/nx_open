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

# Rollback support
class UnitTestRollback:
    _rollbackFile=None
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
            self._rollbackFile.write(("%s,%s,%s\n")%(methodName,s,resourceId))
        self._rollbackFile.flush()

    def _doSingleRollback(self,methodName,serverAddress,resourceId):
        # this function will do a single rollback
        req = urllib2.Request("http://%s/ec2/%s" % (serverAddress,"removeResource"), \
            data=self._removeTemplate%(resourceId), headers={'Content-Type': 'application/json'})
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
        self._rollbackFile.seek(0,0);
        for line in self._rollbackFile:
            if line == '\n':
                continue
            # python will left the last line break character there
            # so we need to wipe it out if we find one there
            l = line.split(',')
            if l[2][len(l[2])-1] == "\n" :
                l[2] = l[2][:-1] # remove last character
            # now we have method,serverAddress,resourceId
            if self._doSingleRollback(l[0],l[1],l[2]) == False:
                failed = True
                # failed for this rollback
                print ("Cannot rollback for transaction:(MethodName:%s;ServerAddress:%s;ResourceId:%s\n)")% (l[0],l[1],l[2])
                print  "Or you could run recover later when all the rollback done\n"
                recoverList.append("%s,%s,%s\n"%(l[0],l[1],l[2]))
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

class ClusterTest():
    clusterTestServerList = []
    clusterTestSleepTime = None
    clusterTestServerUUIDList = []
    threadNumber = 16
    testCaseSize = 2
    unittestRollback = None

    _getterAPIList = [
        "getResourceParams",
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
        "getLicenses"]

    def _callAllGetters(self):
        print "======================================"
        print "Test all ec2 get request status"
        for s in clusterTest.clusterTestServerList:
            for reqName in self._ec2GetRequests:
                print "Connection to http://%s/ec2/%s"%(s,reqName)
                response = urllib2.urlopen("http://%s/ec2/%s" % (s,reqName))
                if response.getcode() != 200:
                    return (False,"%s failed with statusCode %d" % (reqName,response.getcode()))
                response.close()
        print "All ec2 get requests work well"
        print "======================================"
        return (True,"Server:%s test for all getter pass" % (s))

    def __init__(self):
        self._setUpPassword()

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
            start = max(0,i-64)
            end = min(64,len(str)-i)+i
            comp1 = str[start:i]
            comp2 = str[i]
            comp3 = ""
            if i+1 >= len(str):
                comp3 = ""
            else:
                comp3 = str[i+1:end]
            print "%s^^^%s^^^%s\n"%(comp1,comp2,comp3)


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
                print "The first different character is at location:%d"%(i+1)
                return


    def _testConnection(self):
        print "==================================================\n"
        print "Test connection on each server in the server list "
        for s in self.clusterTestServerList:
            print "Try to connect to server:%s"%(s)
            try:
                response = urllib2.urlopen("http://%s/ec2/testConnection"%(s),timeout=5)
            except urllib2.URLError , e:
                print "The connection will be issued with a 5 seconds timeout"
                print "However connection failed with error:%r"%(e)
                print "Connection test failed"
                return False

            if response.getcode() != 200:
                return False
            response.close()
            print "Connection test OK\n"

        print "Connection Test Pass"
        print "\n==================================================="
        return True

    # This checkResultEqual function will categorize the return value from each server
    # and report the difference status of this

    def _reportDetailDiff(self,key_list):
        for i in xrange(len(key_list)):
            for k in xrange(i+1,len(key_list)):
                print "-----------------------------------------"
                print "Group %d compared with Group %d\n"%(i+1,k+1)
                self._seeDiff(key_list[i],key_list[k])
                print "-----------------------------------------"

    def _reportDiff(self,statusDict,methodName):
        print "\n\n**************************************************\n\n"
        print "Report each server status of method:%s\n"%(methodName)
        print "The server will be categorized as group,each server within the same group has same status,while different groups have different status\n"
        print "The total group number is:%d\n"%(len(statusDict))
        if len(statusDict) == 1:
            print "The status check passed!\n"
        i = 1
        key_list = []
        for key in statusDict:
            list = statusDict[key]
            print "Group %d:(%s)\n"%(i,','.join(list))
            i = i +1
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
                    statusDict[content]=[address]
                response.close()

        self._reportDiff(statusDict,methodName)

        if len(statusDict) > 1:
            return (False,"")

        return (True,"")

    def _checkResultEqual_Deprecated(self,responseList,methodName):
        print "------------------------------------------\n"
        print "Test sync status on method:%s"%(methodName)
        result = None
        resultAddr = None
        for entry in responseList:
            response = entry[0]
            address  = entry[1]

            if response.getcode() != 200:
                return (False,"Server:%s method:%s http request failed with code:%d"%(address,methodName,response.getcode()))
            else:
                content = response.read()
                if result == None:
                    result = content
                    resultAddr = address
                else:
                    if content != result:
                        print "Server:%s has different status with server:%s on method:%s"%(address,resultAddr,methodName)
                        self._seeDiff(content,result)
                        return (False,"Failed to sync")
            response.close()
        print "Method:%s is sync in cluster"%(methodName)
        print "\n------------------------------------------"
        return (True,"")

    def checkMethodStatusConsistent(self,method):
            responseList = []
            for server in self.clusterTestServerList:
                print "Connection to http://%s/ec2/%s"%(server, method)
                responseList.append((urllib2.urlopen("http://%s/ec2/%s" % (server, method)),server))

            # checking the last response validation
            return self._checkResultEqual_Deprecated(responseList,method)

    def _ensureServerListStates(self,sleep_timeout):
        time.sleep(sleep_timeout)
        for method in self._getterAPIList:
            ret,reason = self.checkMethodStatusConsistent(method)
            if ret == False:
                return (ret,reason)
        return (True,"")

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
_uniqueCameraSeedNumber =0
_uniqueUserSeedNumber =0
_uniqueMediaServerSeedNumber =0



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

    def _initializeThreadWorker(self):
        for _ in xrange(self._threadNum):
            t = threading.Thread(target=self._worker)
            t.start()
            self._threadList.append(t)

    def join(self):
        # We delay the real operation until we call join
        self._initializeThreadWorker()
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

        ret = "ec2_test_cam_%s_%s"%(_uniqueSessionNumber,_uniqueCameraSeedNumber)
        _uniqueCameraSeedNumber = _uniqueCameraSeedNumber +1
        return ret

    def generateUserName(self):
        global _uniqueSessionNumber
        global _uniqueUserSeedNumber

        ret = "ec2_test_user_%s_%s"%(_uniqueSessionNumber,_uniqueUserSeedNumber)
        _uniqueUserSeedNumber = _uniqueUserSeedNumber + 1
        return ret

    def generateMediaServerName(self):
        global _uniqueSessionNumber
        global _uniqueMediaServerSeedNumber

        ret = "ec2_test_media_server_%s_%s"%(_uniqueSessionNumber,_uniqueMediaServerSeedNumber)
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
                i = i +1

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
        "isAdmin": false,
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
                id,un),id))

        return ret

    def generateUpdateData(self,id):
        un,pwd,digest = self.generateUsernamePasswordAndDigest(self.generateUserName)
        return (self._template % (digest,
                self.generateEmail(),
                self.generatePasswordHash(pwd),
                id,un),id)

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

# This class serves as an in-memory data base. Before doing the confliction test,
# we begining by creating some sort of resources and then start doing the confliction
# test. These data is maintained in a separate dictionary and once everything is done
# it will be rollback.

class ConflictionDataGenerator(BasicGenerator):
    conflictCameraList=[]
    conflictUserList=[]
    conflictMediaServerList=[]
    _lock = threading.Lock()
    _listLock = threading.Lock()
    
    def _prepareData(self,dataList,methodName,l):
        worker = ClusterWorker(8,len(clusterTest.clusterTestServerList)*len(dataList))

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
                        print "Failed to connect to http://%s/ec2/%s"%(server,methodName)
                    else:
                        clusterTest.unittestRollback.addOperations(methodName,server,post_data[1])
                        with list_lock:
                            list.append(post_data[0])
                    response.close() 

                worker.enqueue(task,(self._lock,l,self._listLock,d,s,))

        worker.join()
        return True

    def _prepareCameraData(self,op,num,methodName,l):
        worker = ClusterWorker(8,len(clusterTest.clusterTestServerList)*num)

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
                        print "Failed to connect to http://%s/ec2/%s"%(server,methodName)
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
            self._existedResourceList.append(
                (obj["id"],obj["parentId"],obj["typeId"]))
        # users
        for entry in gen.conflictUserList:
            obj = json.loads(entry)
            self._existedResourceList.append(
                (obj["id"],obj["parentId"],obj["typeId"]))
        # media server
        for entry in gen.conflictMediaServerList:
            obj = json.loads(entry)
            self._existedResourceList.append(
                (obj["id"],obj["parentId"],obj["typeId"]))
        
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
        worker = ClusterWorker(8,num*len(clusterTest.clusterTestServerList))

        for _ in xrange(num):
            for s in clusterTest.clusterTestServerList:

                def task(lock,list,listLock,server,mname,oper):

                    with lock:
                        d = oper(1,s)[0]
                        req = urllib2.Request("http://%s/ec2/%s" % (server,mname),
                                data=d[0], headers={'Content-Type': 'application/json'})
                        response = urllib2.urlopen(req)

                    if response.getcode() != 200:
                        print "Failed to connect http://%s/ec2/%s"%(server,mname)
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
        # We could not use existed camera list since if we do so we may break the existed
        # database on the server side. What we gonna do is just create new fake cameras and
        # then do the test by so.
        json_list = []

        if not self._prepareData( CameraDataGenerator().generateCameraData,num,"saveCameras",json_list):
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

    _existedFakeServerList=[]

    _lock = threading.Lock()
    _listLock = threading.Lock()

    def _prepareData(self,dataList,methodName,l):
        worker = ClusterWorker(8,len(dataList)*len(clusterTest.clusterTestServerList))

        for d in dataList:
            for s in clusterTest.clusterTestServerList:
                
                def task(lock,list,listLock,post_data,mname,server):
                    req = urllib2.Request("http://%s/ec2/%s" % (server,mname),
                        data=post_data[0], headers={'Content-Type': 'application/json'})

                    with lock:
                        response = urllib2.urlopen(req)

                    if response.getcode() != 200:
                        print "Failed to connect http://%s/ec2/%s"%(server,mname)
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

        if not self._prepareData( MediaServerGenerator().generateMediaServerData(num),"saveMediaServer",json_list):
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
            serverName = clusterTest.clusterTestServerList[random.randint(0,len(clusterTest.clusterTestServerList)-1)]
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
            print "Connection to http://%s/ec2/%s"%(server,methodName)
            response = urllib2.urlopen(req)

        # Do a sligtly graceful way to dump the sample of failure
        if response.getcode() != 200:
            self._dumpFailedRequest(d,methodName) 

        self.assertTrue(response.getcode() == 200, \
            "%s failed with statusCode %d" % (methodName,response.getcode()))

        response.close()

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
            s = clusterTest.clusterTestServerList[random.randint(0,len(clusterTest.clusterTestServerList)-1)]
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
        self._gen = ResourceDataGenerator(self._testCase*2)

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
        self._gen = ResourceDataGenerator(self._testCase*2)

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
        self._gen = CameraUserAttributesListDataGenerator(self._testCase*2)

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
        self._gen = ServerUserAttributesListDataGenerator(self._testCase*2)

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
        self._conflictList = [
            ("removeResource","saveMediaServer",MediaServerConflictionDataGenerator(dataGen)),
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
        self._mergeTest  = mt

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
            print "Connection to http://%s/ec2/%s"%(addr,method)
            response = urllib2.urlopen(req)

        if response.getcode() != 200 :
            return (False,"Cannot issue %s with HTTP code:%d to server:%s" % (method,response.getcode(),addr))

        response.close()

        return (True,"")

    def _setSystemName(self,addr,name):
        query = { "systemName":name }
        response = None

        with self._mergeTest._lock:
            response = urllib2.urlopen("http://%s/api/configure?%s"%(addr,urllib.urlencode(query)))

        if response.getcode() != 200:
            return (False,"Cannot issue changeSystemName request to server:%s"%(addr))

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

    def main(self,addr,name):
        # 1, change the system name
        self._setSystemName(addr,name)
        # 2, generate random data to this server
        ret,reason = self._generateRandomStates(addr)
        if ret == False:
            raise Exception("Cannot generate random states:%s" % (reason))
    
# This class is used to control the whole merge test
class MergeTest:
    _systemName = "mergeTest"
    _gen = BasicGenerator()

    _lock = threading.Lock()

    _serverOldSystemNameList=[]
    _mergeTestTimeout = 1

    def _prolog(self):
        print "Merge test prolog : Test whether all servers you specify has the identical system name"
        for s in clusterTest.clusterTestServerList:
            print "Connection to http://%s/ec2/testConnection"%(s)
            response = urllib2.urlopen("http://%s/ec2/testConnection"%(s))
            if response.getcode() != 200:
                return False
            jobj = json.loads( response.read() )
            self._serverOldSystemNameList.append(jobj["systemName"])
            response.close()

        print "Merge test prolog pass"
        return True
    
    def _epilog(self):
        print "Merge test epilog, change all servers system name back to its original one"
        idx = 0
        for s in clusterTest.clusterTestServerList:
            self._setSystemName(s,self._serverOldSystemNameList[idx])
            idx = idx+1
        print "Merge test epilog done"

    # This function will ENSURE that the random system name is unique
    def _generateRandomSystemName(self):
        ret = []
        s = set()
        for i in xrange(len(clusterTest.clusterTestServerList)):
            length = random.randint(16,30)
            while True:
                name = self._gen.generateRandomString(length)
                if name in s or name == self._systemName:
                    continue
                else:
                    s.add(name)
                    ret.append((clusterTest.clusterTestServerList[i],name))
                    break
        return ret

    # First phase will make each server has its own status
    # and also its unique system name there

    def _phase1(self):
        print "Merge test phase1: generate UNIQUE system name for each server and do modification"
        testList = self._generateRandomSystemName()
        worker = ClusterWorker(clusterTest.threadNumber,len(testList))

        for entry in testList:
            worker.enqueue(PrepareServerStatus(self).main,
                (entry[0],entry[1],))

        worker.join()
        print "Merge test phase1 done, now sleep and wait for sync"
        time.sleep(self._mergeTestTimeout)

    # Second phase will make each server has the same system
    # name which triggers server merge operations there.

    def _setSystemName(self,addr,name):

        print "Connection to http://%s/api/configure"%(addr)

        with self._lock:
            response = urllib2.urlopen(
                "http://%s/api/configure?%s"%(addr,urllib.urlencode({"systemName":name})))

        if response.getcode() != 200 :
            return (False,"Cannot issue changeSystemName with HTTP code:%d to server:%s" % (response.getcode()),addr)

        response.close()

        return (True,"")

    def _setToSameSystemName(self):
        worker = ClusterWorker(clusterTest.threadNumber,len(clusterTest.clusterTestServerList))
        for entry in  clusterTest.clusterTestServerList:
            worker.enqueue(self._setSystemName,(entry,self._systemName,))
        worker.join()

    def _phase2(self):
        print "Merge test phase2: set ALL the servers with system name :mergeTest"
        self._setToSameSystemName()
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

    def _loadConfig(self):
        config_parser = ConfigParser.RawConfigParser()
        config_parser.read("ec2_tests.cfg")
        self._mergeTestTimeout = config_parser.getint("General","mergeTestTimeout")

    def test(self):
        print "================================\n"
        print "Server Merge Test Start\n"

        self._prolog()
        self._phase1()
        self._phase2()
        self._epilog()

        print "Server Merge Test End\n"
        print "================================\n"

# ===================================
# RTSP test
# ===================================

class RRRtspTcp:
    _socket = None
    _addr = None
    _port = None
    _data = None
    _cid = None
    _sid = None
    _mac = None
    _uname = None
    _pwd = None

    _rtspBasicTemplate =  "PLAY rtsp://%s/%s RTSP/1.0\r\n\
        CSeq: 2\r\n\
        Range: npt=now-\r\n\
        Scale: 1\r\n\
        x-guid: %s\r\n\
        Session:\r\n\
        User-Agent: Network Optix\r\n\
        x-play-now: true\r\n\
        Authorization: Basic YWRtaW46MTIz\r\n\
        x-server-guid: %s\r\n\r\n"

    _rtspDigestTemplate = "PLAY rtsp://%s/%s RTSP/1.0\r\n\
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
    
    def __init__(self,addr,port,mac,cid,sid,uname,pwd):
        self._port = port
        self._addr = addr
        self._data = self._rtspBasicTemplate%(
            "%s:%d"%(addr,port),
            mac,
            cid,
            sid)

        self._cid = cid
        self._sid = sid
        self._mac = mac
        self._uname = uname
        self._pwd = pwd

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
            if idx_end <0:
                return False
        nonce = reply[idx_start+1:idx_end-1]

        return (realm,nonce)

    # This function only calcuate the digest response
    # not the specific header field. So another format
    # is required to format the header into the target
    # HTTP digest authentication

    def _generateMD5String(self,m):
        return ''.join('%02x' % ord(i) for i in m)

    def _calDigest(self,realm,nonce):
        m = md5.new()
        m.update( "%s:%s:%s"%(self._uname,realm,self._pwd) )
        part1 = m.digest()

        m = md5.new()
        m.update( "PLAY:/%s"%(self._mac) )
        part2 = m.digest()

        m = md5.new()
        m.update("%s:%s:%s"%(self._generateMD5String(part1),nonce,self._generateMD5String(part2)))
        resp = m.digest()

        # represent the final digest as ANSI printable code
        return self._generateMD5String(resp)


    def _formatDigestHeader(self,realm,nonce):
        resp = self._calDigest(realm,nonce)
        return self._digestAuthTemplate%(
            self._uname,
            realm,
            nonce,
            "/%s"%(self._mac),
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
        data = self._rtspDigestTemplate%(
            "%s:%d"%(self._addr,self._port),
            self._mac,
            self._cid,
            auth,
            self._sid)
        self._request(data)
        return self._response()


    def _checkAuthorization(self,data):
        return data.find("Unauthorized") <0
    
    
    def _request(self,data):
        while True:
            sz = self._socket.send(data)
            if sz == len(data):
                return
            else:
                data = data[sz:] 
            
    def _response(self):
        ret = ""
        while True:
            data = self._socket.recv(1024)
            if not data:
                return ret
            else:
                ret += data
                if self._checkEOF(ret):
                    return ret

    def __enter__(self):
        self._socket = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        self._socket.connect((self._addr,self._port))
        self._request(self._data)
        reply = self._response()
        if not self._checkAuthorization(reply):
            return self._requestWithDigest(reply)
        else:
            return reply
        
    def __exit__(self,type,value,trace):
        self._socket.close()

class SingleServerRtspTest():
    _testCase = 0 
    """ The RTSP test case number """

    _serverEndpoint = None
    _serverGUID = None
    _cameraList = []
    _cameraInfoTable = dict()
    _testCase = 0
    _username = None
    _password = None

    def __init__(self,serverEndpoint,serverGUID,testCase,uname,pwd):
        self._serverEndpoint = serverEndpoint
        self._serverGUID = serverGUID
        self._testCase = testCase
        self._username = uname
        self._password = pwd

        self._fetchCameraList()

    def _fetchCameraList(self):
        response = urllib2.urlopen("http://%s/ec2/getCameras" % (self._serverEndpoint))

        if response.getcode() != 200:
            raise Exception("Cannot connect to server:%s using getCameras" % (self._serverEndpoint))
        json_obj = json.loads(response.read())

        for c in json_obj:
            self._cameraList.append((c["mac"],c["id"]))
            self._cameraInfoTable[c["id"]] = c

        response.close()
        
    def _checkReply(self,reply):
        idx = reply.find("\r\n")
        if idx < 0:
            return False
        else:
            return "RTSP/1.0 200 OK" == reply[:idx]

    def _testMain(self):
        c = self._cameraList[random.randint(0,len(self._cameraList) - 1)]
        l = self._serverEndpoint.split(":")

        with RRRtspTcp(l[0],int(l[1]),
                     c[0],
                     c[1],
                     self._serverGUID,
                     self._username,
                     self._password) as reply:

            assert self._checkReply(reply),"Server:%s RTSP failed, with reply:%s" % (self._serverEndpoint,reply)

    def run(self):
        worker = ClusterWorker(clusterTest.threadNumber , self._testCase)
        for _ in xrange(self._testCase):
            worker.enqueue(self._testMain,())
        worker.join()	

class ServerRtspTest:
    _testCase = 0
    _username = None
    _password = None
    
    def __init__(self):
        p = ConfigParser.RawConfigParser()
        p.read("ec2_tests.cfg")
        self._testCase = p.getint("Rtsp","testSize")
        self._username = p.get("General","username")
        self._password = p.get("General","password")
        
    def test(self):
        for i in xrange(len(clusterTest.clusterTestServerList)):
            serverAddr = clusterTest.clusterTestServerList[i]
            serverAddrGUID = clusterTest.clusterTestServerUUIDList[i][0]
            SingleServerRtspTest(serverAddr,serverAddrGUID,self._testCase,
                              self._username,self._password).run()


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

def doClearAll():
    MediaServerOperation().removeAll();
    UserOperation().removeAll();
    MediaServerOperation().removeAll()


# ===================================
# Perf Test
# ===================================

class PerfTest:
    _initialCreationSize = 20
    _frequency = 0
    _newUserList = []
    _newCameraList=[]
    _exit = False
    _queue= None
    _threadPool=[]

    # statistics
    _userCreateNeg = 0
    _userCreatePos = 0
    _cameraCreateNeg=0
    _cameraCreatePos=0
    _userUpdateNeg=0
    _userUpdatePos=0
    _cameraUpdateNeg=0
    _cameraUpdatePos=0

    _creationProb = 0.3

    _lock = threading.Lock()

    def _onInterrupt(self,a,b):
        self._exit = True

    # prepare the data for the performance test. This serves as the initial
    # pool where we start our testing 

    def _doCreate(self,addr,d,id,methodName):
        req = urllib2.Request("http://%s/ec2/%s" % (addr,methodName),
                        data=d, headers={'Content-Type': 'application/json'})

        response = None

        with self._lock:
            response = urllib2.urlopen(req)

        if response.getcode() != 200:
            # failed 
            return False
        else:
            if id != None:
                clusterTest.unittestRollback.addOperations(methodName,addr,id)

        response.close()

        return True

    def _prepareData(self,d,methodName,addr):
        if not self._doCreate(addr,d[0],d[1],methodName):
            return False
        return True

    def _prepare(self,num):
        userList = UserDataGenerator().generateUserData(num)
        cameraList=[]
        for s in clusterTest.clusterTestServerList:
            for d in userList:
                if not self._prepareData(d,"saveUser",s):
                    return False

        cameraGen = CameraDataGenerator()
        for s in clusterTest.clusterTestServerList:
           for _ in xrange(num):
               c = cameraGen.generateCameraData(1,s)[0]
               cameraList.append(c)
               if not self._prepareData(c,"saveCameras",s):
                   return False

        self._userCreatePos  = num
        self._cameraCreatePos= num

        for u in userList:
            self._newUserList.append(u[1])

        for c in cameraList:
            self._newCameraList.append(c[1])

        return True

    def _takePlace(self,prob):
        if random.random() <= prob:
            return True
        else:
            return False

    def _threadMain(self):
        uGen = UserDataGenerator()
        cGen = CameraDataGenerator()

        while not self._exit:
            try:
                _ = self._queue.get(True,1)
            except:
                continue
            if self._takePlace(self._creationProb):
                if random.randint(0,1) == 0:
                    failed = False
                    for s in clusterTest.clusterTestServerList:
                        d = uGen.generateUserData(1)
                        if not self._doCreate(s,d[0][0],d[0][1],"saveUser"):
                            failed = True
                        else:
                            self._newUserList.append(d[0][1])

                    if failed:
                        self._userCreateNeg = self._userCreateNeg+1
                    else:
                        self._userCreatePos = self._userCreatePos+1

                else:
                    failed = False
                    for s in clusterTest.clusterTestServerList:
                        d = cGen.generateCameraData(1,s)
                        if not self._doCreate(s,d[0][0],d[0][1],"saveCameras"):
                            failed = True
                        else:
                            self._newCameraList.append(d[0][1])

                    if failed:
                        self._cameraCreateNeg = self._cameraCreateNeg+1
                    else:
                        self._cameraCreatePos = self._cameraCreatePos+1

            else:
                if random.randint(0,1) == 0:
                    failed = False
                    for s in clusterTest.clusterTestServerList:
                        id = self._newUserList[random.randint(0,len(self._newUserList)-1)]
                        d = uGen.generateUpdateData(id)
                        if not self._doCreate(s,d[0][0],None,"saveUser"):
                            failed = True
                        else:
                            failed = False

                    if failed:
                        self._userUpdateNeg = self._userUpdateNeg+1
                    else:
                        self._userUpdatePos = self._userUpdatePos+1
                else:
                    failed = False
                    for s in clusterTest.clusterTestServerList:
                        id = self._newCameraList[random.randint(0,len(self._newCameraList)-1)]
                        d = cGen.generateUpdateData(id,s)
                        if not self._doCreate(s,d[0][0],None,"saveCameras"):
                            failed = True
                        else:
                            failed = False

                    if failed:
                        self._cameraUpdateNeg = self._cameraUpdateNeg+1
                    else:
                        self._cameraUpdatePos = self._cameraUpdatePos+1

    def _initThreadPool(self,num):
        expectedQueueSize = self._frequency * 10;
        self._queue = Queue.Queue(expectedQueueSize)

        for _ in xrange(num):
            th = threading.Thread(target=self._threadMain)
            th.start()
            self._threadPool.append(th)

    def _pollOnce(self):
        start = time.time()
        for _ in xrange(self._frequency):
            try:
                self._queue.put((),True,1)
            except:
                pass
            if self._exit :
                return True
        end = time.time()
        if end-start < 1.0:
            try:
                time.sleep(1.0-(end-start))
            except:
                pass # interruption 

    def start(self):
        print "Start to prepare performance test, do not interrupt"
        # prepare the initial stage
        if not self._prepare(self._initialCreationSize):
            print "The performance test initialization cannot be done, check server status"
            return False
        # install the signal handler , thread pool and configuration
        cfg_parser = ConfigParser.RawConfigParser()
        cfg_parser.read("ec2_tests.cfg")
        self._frequency = cfg_parser.getint("PerfTest","frequency")

        try:
            self._creationProb = float(cfg_parser.get("PerfTest","createProb"))
        except:
            pass

        self._initThreadPool(16)
        signal.signal(signal.SIGINT,self._onInterrupt)
        
        print "Performance test start,press ctrl+c to stop"
        loop_start = time.time()
        # start the loop here and recording the time
        while not self._exit:
            if self._pollOnce() :
                break

        # join all the threads
        for th in self._threadPool:
            th.join()

        loop_end = time.time()

        print "=============================== Perf test done ======================================"
        print "Successful camera creation:%d"%(self._cameraCreatePos)
        print "Failed camera creation:%d"%(self._cameraCreateNeg)
        print "Successful camera update:%d"%(self._cameraUpdatePos)
        print "Failed camera update:%d"%(self._cameraUpdateNeg)
        print "Successful user creation:%d"%(self._userCreatePos)
        print "Failed user creation:%d"%(self._userCreateNeg)
        print "Successful user update:%d"%(self._userUpdatePos)
        print "Failed user update:%d"%(self._userUpdateNeg)
        print "Total execution time:%d seconds"%(loop_end-loop_start)
        print "======================================================================================"

        try:
            raw_input("Press any key to continue rollback...")
        except:
            return True

        return True

def runPerformanceTest():
    if len(sys.argv) != 3 and len(sys.argv) != 2 :
        return (False,"2 or 1 parameters are needed")

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
            else:
                return (False,"--remove can only have --id options")
        elif len(sys.argv) == 2:
            if t.removeAll() == False:
                return (False,"cannot perform remove all operation")
    else:
        return (False,"Unknown command:%s" % (l[0]))

    return True

class SystemNameTest:
    _serverList=[]
    _guidDict=dict()
    _oldSystemName=None
    _syncTime=2

    def _setUpAuth(self,user,pwd):
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for s in self._serverList:
            passman.add_password("NetworkOptix","http://%s/ec2/" % (s), user, pwd)
            passman.add_password("NetworkOptix","http://%s/api/" % (s), user, pwd)

        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

    def _doGet(self,addr,methodName):
        ret = None
        response = None
        print "Connection to http://%s/ec2/%s"%(addr,methodName)
        response = urllib2.urlopen("http://%s/ec2/%s"%(addr,methodName))

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

        print "Connection to http://%s/ec2/%s"%(addr,methodName)
        response = urllib2.urlopen(req)

        if response.getcode() != 200:
            response.close()
            return False
        else:
            response.close()
            return True

    def _changeSystemName(self,addr,name):
        response = urllib2.urlopen("http://%s/api/configure?%s"%(addr,urllib.urlencode({"systemName":name})))
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
                return (False,"Server:%s cannot perform testConnection REST call"%(s))
            obj = json.loads(ret)

            if systemName != None:
                if systemName != obj["systemName"]:
                    return (False,"Server:%s has systemName:%s which is different with others:%s"%(s,obj["systemName"],systemName))
            else:
                systemName=obj["systemName"]

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
            return (False,"Server:%s cannot doGet on getMediaServersEx"%(s))
        obj = json.loads(ret)
        for ele in obj:
            if ele["id"] == thisGUID:
                if ele["status"] == "Offline":
                    return (False,"This server:%s with GUID:%s should be Online"%(s,thisGUID))
            else:
                if ele["status"] == "Online":
                    return (False,"The server that has IP address:%s and GUID:%s \
                    should be Offline when login on Server:%s"%(ele["networkAddresses"],ele["id"],s))
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


helpStr="Usage:\n\n" \
    "--perf : Start performance test.User can use ctrl+c to interrupt the perf test and statistic will be displayed.User can also specify configuration parameters " \
    "for performance test. In ec2_tests.cfg file,\n[PerfTest]\nfrequency=1000\ncreateProb=0.5\n, the frequency means at most how many operations will be issued on each" \
    "server in one seconds(not guaranteed,bottleneck is CPU/Bandwidth for running this script);and createProb=0.5 means the creation operation will be performed as 0.5 "\
    "probability, and it implicitly means the modification operation will be performed as 0.5 probability \n\n" \
    "--clear: Clear all the Cameras/MediaServers/Users on all the servers.It will not delete admin user. \n\n" \
    "--sync: Test all the servers are on the same page or not.This test will perform regarding the existed ec2 REST api, for example " \
    ", no layout API is supported, then this test cannot test whether 2 servers has exactly same layouts  \n\n" \
    "--recover: Recover from last rollback failure. If you see rollback failed for the last run, you can run this option next time to recover from " \
    "the last rollback error specifically. Or you could run any other cases other than --help/--recover,the recover will be performed automatically as well. \n\n" \
    "--merge-test: Test server merge. The user needs to specify more than one server in the config file. This test will temporarilly change the server system name," \
    "currently I assume such change will NOT modify the server states. Once the merge test finished, the system name for each server will be recover automatically.\n\n" \
    "--rtsp-test: Test the rtsp streaming. The user needs to specify section [Rtsp] in side of the config file and also " \
    "testSize attribute in it , eg: \n[Rtsp]\ntestSize=100\n Which represent how many test case performed on EACH server.\n\n" \
    "--add=Camera/MediaServer/User --count=num: Add a fake Camera/MediaServer/User to the server you sepcify in the list. The --count " \
    "option MUST be specified , so a working example is like: --add=Camera --count=500 . This will add 500 cameras(fake) into "\
    "the server.\n\n"\
    "--remove=Camera/MediaServer/User (--id=id)*: Remove a Camera/MediaServer/User with id OR remove all the resources.The user can "\
    "specify --id option to enable remove a single resource, eg : --remove=MediaServer --id={SomeGUID} , and --remove=MediaServer will remove " \
    "all the media server.\nNote: --add/--remove will perform the corresponding operations on a random server in the server list if the server list "\
    "have more than one server.\n\n" \
    "If no parameter is specified, the default automatic test will performed.This includes add/update/remove Cameras/MediaServer/Users and also add/update " \
    "user attributes list , server attributes list and resource parameter list. Additionally , the confliction test will be performed as well, it includes " \
    "modify a resource on one server and delete the same resource on another server to trigger confliction.Totally 9 different test cases will be performed in this " \
    "run.Currently, all the modification/deletion will only happened on fake data, and after the whole testing finished, the fake data will be wiped out so " \
    "the old database should not be modified.The user can specify configuration parameter in ec2_tests.cfg file, eg:\n[General]\ntestCaseSize=200\nclusterTestSleepTime=10\n "\
    "this options will make each test case in 9 cases run on each server 200 times. Additionally clusterTestSleepTime represent after every 200 operations, how long should I "\
    "wait and then perform sync operation to check whether all the server get the notification"

def doCleanUp():
    selection = None
    try :
        selection = raw_input("Press Enter to continue ROLLBACK or press x to SKIP it...")
    except:
        pass

    if len(selection) == 0 or selection[0] != "x":
        print "Now do the rollback, do not close the program!"
        clusterTest.unittestRollback.doRollback()
        print "++++++++++++++++++ROLLBACK DONE+++++++++++++++++++++++"
    else:
        print "Skip ROLLBACK,you could use --recover to perform manually rollback"
            
if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == '--help':
        print helpStr
    elif len(sys.argv) == 2 and sys.argv[1] == '--recover':
        UnitTestRollback().doRecover()
    elif len(sys.argv) == 2 and sys.argv[1] == '--sys-name':
        SystemNameTest().run()
    else:
        print "The automatic test starts,please wait for checking cluster status,test connection and APIs..."
        # initialize cluster test environment
        ret,reason = clusterTest.init()
        if ret == False:
            print "Failed to initialize the cluster test object:%s" % (reason)
        elif len(sys.argv) == 2 and sys.argv[1] == '--sync':
            pass # done here, since we just need to test whether 
                 # all the servers are on the same page
        else:
            if len(sys.argv) == 1:
                try:
                    unittest.main()
                except:
                    MergeTest().test()
                    SystemNameTest().run()

                    print "\n\nALL AUTOMATIC TEST ARE DONE\n\n"
                    doCleanUp()

            elif len(sys.argv) == 2 and sys.argv[1] == '--clear':
                doClearAll()
            elif len(sys.argv) == 2 and sys.argv[1] == '--perf':
                PerfTest().start()
                doCleanUp()
            else:
                if sys.argv[1] == '--merge-test':
                    MergeTest().test()
                    doCleanUp()

                elif sys.argv[1] == '--rtsp-test':
                    ServerRtspTest().test()
                else:
                    ret = runPerformanceTest()
                    doCleanUp()
                    if ret != True:
                        print ret[1]