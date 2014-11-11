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

class ClusterTest():
    clusterTestServerList=[]
    clusterTestSleepTime=None
    clusterTestServerUUIDList=[]

    _getterAPIList = [
        "getResourceTypes",
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
        "getFullInfo",
        "getLicenses"
    ]

    def _getServerName(self,obj,uuid):
        for s in obj:
            if s["id"] == uuid:
                return s["name"]
        return None

    # call this function after reading the clusterTestServerList
    def _fetchClusterTestServerUUIDList(self):

        server_id_list=[]
        for s in self.clusterTestServerList:
            response = urllib2.urlopen(
                "http://%s/ec2/testConnection?format=json"%(s))
            if response.getcode() != 200:
                return (False,"testConnection response with error code:%d"%(response.getcode()))
            json_obj = json.loads( response.read() )
            server_id_list.append(json_obj["ecsGuid"])


        # We still need to get the server name since this is useful for 
        # the SaveServerAttributeList test (required in its post data)

        response = urllib2.urlopen(
            "http://%s/ec2/getMediaServersEx?format=json"%(self.clusterTestServerList[0]))

        if response.getcode() != 200:
            return (False,"getMediaServersEx resposne with error code:%d"%(response.getcode()))

        json_obj = json.loads( response.read() )

        for uuid in server_id_list:
            n = self._getServerName(json_obj,uuid)
            self.clusterTestServerUUIDList.append((uuid,n))

        return (True,"")

    def _checkResultEqual(self,responseList,methodName):
        result=None
        for response in responseList:
            if response.getcode() != 200:
                return(False,"method:%s http request failed with code:%d"%
                    (methodName,response.getcode))
            else:
                # painfully slow, just bulk string comparison here
                if result == None:
                    result=response.read()
                else:
                    output = response.read()
                    if result != output:
                        return(False,"method:%s return value differs from other"%
                            (methodName))
        return (True,"")

    def checkMethodStatusConsistent(self,method):
            responseList=[]
            for server in self.clusterTestServerList:
                responseList.append(
                    (urllib2.urlopen( "http://%s/ec2/%s"%( server, method))))

            # checking the last response validation 
            return self._checkResultEqual(responseList,method)

    def _ensureServerListStates(self,sleep_timeout):
        time.sleep(sleep_timeout)
        for method in self._getterAPIList:
            ret,reason = self.checkMethodStatusConsistent(method)
            if ret == False:
                return (ret,reason)
        return (True,"")

    def _loadConfig(self):
        parser = ConfigParser.RawConfigParser();
        parser.read("ec2_tests.cfg")
        self.clusterTestServerList = parser.get("General","serverList").split(",")
        self.clusterTestSleepTime = parser.getint("General","clusterTestSleepTime")
        return (parser.get("General","password"),parser.get("General","username"))

    def init(self):
        pwd,un=self._loadConfig()
        # configure different domain password strategy
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for s in self.clusterTestServerList:
            passman.add_password( "NetworkOptix","http://%s/ec2/" %(s), un, pwd )
        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

        # ensure all the server are on the same page
        ret,reason = self._ensureServerListStates(self.clusterTestSleepTime)

        if ret == False:
            return (ret,reason)

        ret,reason = self._fetchClusterTestServerUUIDList()

        if ret == False:
            return (ret,reason)

        return (True,"")

clusterTest=ClusterTest()

class EnumerateAllEC2GetCallsTest():
    _ec2GetRequests = [
        "getResourceTypes",
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
        "getLicenses"
    ]

    def test_callAllGetters(self):
        for s in clusterTest.clusterTestServerList:
            for reqName in ec2GetRequests:
                response = urllib2.urlopen("http://%s/ec2/%s" % (s,reqName))
                self.assertTrue(response.getcode() == 200, \
                    "%s failed with statusCode %d" % (reqName,response.getcode()))
            print "Server:%s test for all getter pass"%(s)


class BasicGenerator():
    # generate MAC address of the object
    def generateMac(self):
        l=[]
        for i in range(0,5):
            l.append(str(random.randint(0,255))+'-')
        l.append(str(random.randint(0,255)))
        return ''.join(l)

    def generateTrueFalse(self):
        if random.randint(0,1) == 0:
            return "false"
        else:
            return "true"

    def generateRandomString(self,length):
        chars=string.ascii_uppercase + string.digits
        return ''.join(random.choice(chars) for _ in range(length))

    def generateUUIdFromMd5(self,salt):
        m=md5.new()
        m.update(salt)
        v = m.digest()
        return "{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}" \
            %(ord(v[0]),ord(v[1]),ord(v[2]),ord(v[3]),
              ord(v[4]),ord(v[5]),ord(v[6]),ord(v[7]),
              ord(v[8]),ord(v[9]),ord(v[10]),ord(v[11]),
              ord(v[12]),ord(v[13]),ord(v[14]),ord(v[15]))
        
    def generateRandomId(self):
        length = random.randint(6,12)
        salt=self.generateRandomString(length)
        return self.generateUUIdFromMd5(salt)

    def generateIpV4(self):
        return "%d.%d.%d.%d"%(
            random.randint(0,255),
            random.randint(0,255),
            random.randint(0,255),
            random.randint(0,255))

    def generateIpV4Endpoint(self):
        return "%s:%d"%(self.generateIpV4(),random.randint(0,65535))

    def generateEmail(self):
        len = random.randint(6,20)
        user_name = self.generateRandomString(len)
        return "%s@gmail.com"%(user_name)

    def generateEnum(self,*args):
        idx = random.randint(0,len(args)-1)
        return args[idx]

    def generateUsernamePasswordAndDigest(self):
        un_len = random.randint(4,12)
        pwd_len= random.randint(8,20)

        un = self.generateRandomString(un_len)
        pwd= self.generateRandomString(pwd_len)

        m = md5.new()
        m.update("%s:NetworkOptix:%s"%(un,pwd))
        d = m.digest()

        return (un,pwd,''.join('%02x'%ord(i) for i in d))
    
    def generatePasswordHash(self,pwd):
        salt = "%x"%(random.randint(0,4294967295))
        m=md5.new()
        m.update(salt)
        m.update(pwd)
        md5_digest = ''.join('%02x'%ord(i) for i in m.digest())
        return "md5$%s$%s"%(salt,md5_digest)

class CameraDataGenerator(BasicGenerator):
    _template="""[
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
            "parentId": "{47bf37a0-72a6-2890-b967-5da9c390d28a}",
            "physicalId": "%s",
            "preferedServerId": "{00000000-0000-0000-0000-000000000000}",
            "scheduleEnabled": false,
            "scheduleTasks": [ ],
            "secondaryStreamQuality": "SSQualityLow",
            "status": "Unauthorized",
            "statusFlags": "CSF_NoFlags",
            "typeId": "{1657647e-f6e4-bc39-d5e8-563c93cb5e1c}",
            "url": "%s",
            "vendor": "%s"
        }
    ]"""

    def _generateCameraId(self,mac):
        return self.generateUUIdFromMd5(mac)

    def generateCameraData(self,number):
        ret = []
        for i in range(number):
            mac = self.generateMac()
            id = self._generateCameraId(mac)
            name_and_model= self.generateRandomString(6)
            ret.append( self._template%(
                    self.generateTrueFalse(),
                    self.generateTrueFalse(),
                    id,
                    mac,
                    name_and_model,
                    name_and_model,
                    mac,
                    self.generateIpV4(),
                    self.generateRandomString(4)))

        return ret

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
        ret=[]
        for i in range(number):
            un,pwd,digest = self.generateUsernamePasswordAndDigest()
            ret.append(self._template%(
                digest,
                self.generateEmail(),
                self.generatePasswordHash(pwd),
                self.generateRandomId(),
                un))

        return ret

class MediaServerGenerator(BasicGenerator):
    _template= """
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
        for i in range(number):
            ret.append(
               self._template%(
                   self.generateIpV4Endpoint(),
                   self.generateRandomId(),
                   self.generateRandomId(),
                   self._generateRandomName(),
                   self._generateRandomName(),
                   self.generateIpV4Endpoint()))

        return ret

class ResourceDataGenerator(BasicGenerator):

    _ec2ResourceGetter=[
        "getCameras",
        "getMediaServersEx",
        "getUsers"
        ]

    _resourceParEntryTemplate= """
        {
            "name":"%s",
            "resourceId":"%s",
            "value":"%s"
        }
    """

    _resourceSaveTemplate= """
        {
            "id":"%s",
            "name":"%s",
            "parentId":"%s",
            "typeId":"%s",
            "url":"%s"
        }
    """

    _resourceRemoveTemplate = """
        {
            "id":"%s"
        }
    """

    # this list contains all the existed resource that I can find.
    # The method for finding each resource is based on the API in
    # the list _ec2ResourceGetter. What's more , the parentId, typeId
    # and resource name will be recorded (Can be None).

    _existedResourceList=[]

    
    # this function is used to retrieve the resource list on the 
    # server side. We just retrieve the resource list from the 
    # very first server since each server has exact same resource

    def _retrieveResourceUUIDList(self):
        for api in self._ec2ResourceGetter:
            response = urllib2.urlopen(
                    "http://%s/ec2/%s?format=json"%(
                        clusterTest.clusterTestServerList[0],
                        api))

            if response.getcode() != 200:
                continue
            json_obj = json.loads( response.read() )
            for ele in json_obj:
                self._existedResourceList.append(
                    (ele["id"],ele["parentId"],ele["typeId"]))

        if len(self._existedResourceList) == 0:
            return False
        else:
            return True
        
    
    def __init__(self):
        ret = self._retrieveResourceUUIDList()
        if ret == False:
            raise Exception("cannot retrieve resources list on server side")

    def _generateKeyValue(self,uuid):
        key_len = random.randint(4,12)
        val_len = random.randint(24,512)
        return self._resourceParEntryTemplate%(
            self.generateRandomString(key_len),
            uuid,
            self.generateRandomString(val_len))

    def _getRandomResourceUUID(self):
        idx = random.randint(0,len(self._existedResourceList)-1)
        return self._existedResourceList[idx][0]

    def _generateOneResourceParams(self):
        uuid = self._getRandomResourceUUID()
        kv_list=["["]
        num = random.randint(2,20)
        for i in range(num-1):
            num = random.randint(2,20)
            kv_list.append(
                self._generateKeyValue(uuid))
            kv_list.append(",")

        kv_list.append(self._generateKeyValue(uuid))
        kv_list.append("]")

        return ''.join(kv_list)
    
    def generateResourceParams(self,number):
        ret=[]
        for i in range(number):
            ret.append(self._generateOneResourceParams())
        return ret

    def generateSaveResource(self,number):
        ret = []
        # get one existed resource from the server 
        res = self._existedResourceList[
            random.randint(0,len(self._existedResourceList)-1)]

        for i in range(number):
            ret.append(
                self._resourceSaveTemplate%(
                res[0],
                self.generateRandomString(random.randint(4,12)),
                res[1],
                res[2],
                self.generateIpV4()))

        return ret

    def generateRemoveResource(self,number):
        ret = []
        for i in range(number):
            ret.append( self._resourceRemoveTemplate%( self._getRandomResourceUUID() ) )
        return ret


    def generateResourceConfliction(self,number):
        ret = []
        for _ in range(number):
            res = self._existedResourceList[
                random.randint(0,len(self._existedResourceList)-1)]
            ret.append((
                # resource save template
                self._resourceSaveTemplate%(
                res[0],
                self.generateRandomString(random.randint(4,12)),
                res[1],
                res[2],
                self.generateIpV4()),
                # resource remove template
                self._resourceRemoveTemplate%(res[0])))

        return ret

class CameraUserAttributesListDataGenerator(BasicGenerator):
    _template="""
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

    _existedCameraUUIDList=[]

    def __init__(self):
        if self._fetchExistedCameraUUIDList() == False:
            raise Exception("cannot fetch camera list on server")

    def _fetchExistedCameraUUIDList(self):
        response = urllib2.urlopen(
            "http://%s/ec2/getCameras?format=json"%(clusterTest.clusterTestServerList[0]))
        if response == None or response.getcode() != 200:
            return False

        ret = json.loads(response.read())

        for ele in ret:
            self._existedCameraUUIDList.append(
                (ele["id"],ele["name"]))

        return True

    def _getRandomServerId(self):
        idx = random.randint(0,len(clusterTest.clusterTestServerUUIDList)-1)
        return clusterTest.clusterTestServerUUIDList[idx][0]

    def _generateNormalizeRange(self):
        return str(random.random())

    def _generateDewarpingPar(self):
        return self._dewarpingTemplate%(
            self.generateTrueFalse(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange())

    def _getRandomCameraUUIDAndName(self):
        return self._existedCameraUUIDList[
            random.randint(0,len(self._existedCameraUUIDList)-1)]

    def generateCameraUserAttribute(self,number):
        ret=[]

        for i in range(number):
            uuid , name = self._getRandomCameraUUIDAndName()
            ret.append(
                self._template%(
                    self.generateTrueFalse(),
                    uuid,name,
                    self.generateTrueFalse(),
                    self._generateDewarpingPar(),
                    self._getRandomServerId(),
                    self.generateTrueFalse()))

        return ret

class ServerUserAttributesListDataGenerator(BasicGenerator):
    _template="""
    [
        {
            "allowAutoRedundancy": %s,
            "maxCameras": %s,
            "serverID": "%s",
            "serverName": "%s"
        }
    ]
    """

    def _getRandomServer(self):
        idx = random.randint(0,len(clusterTest.clusterTestServerUUIDList)-1)
        return clusterTest.clusterTestServerUUIDList[idx]

    def generateServerUserAttributesList(self,number):
        ret=[]
        for i in range(number):
            uuid,name = self._getRandomServer()
            ret.append(
                self._template%(
                    self.generateTrueFalse(),
                    random.randint(0,200),
                    uuid,name))
        return ret

# Thread queue for multi-task. This is useful since if the user
# want too many data to be sent to the server, then there maybe
# thousands of threads to be created, which is not something we
# want it. This is not a real-time queue, but a push-sync-join

class ClusterWorker():
    _queue=None
    _threadList=None
    _threadNum=1

    def __init__(self,num,element_size):
        self._queue = Queue.Queue(element_size)
        self._threadList = []
        self._threadNum = num
        if element_size < num:
            self._threadNum = element_size

    def _worker(self):
        while not self._queue.empty():
            t,a=self._queue.get(True)
            t(*a)

    def _initializeThreadWorker(self):
        for _ in range(self._threadNum):
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


class ClusterTestBase(unittest.TestCase):
    def _generateModifySeq(self):
        return None

    def _getMethodName(self):
        pass

    def _getObserverName(self):
        pass

    def _defaultModifySeq(self,fakeData):
        ret=[]
        for f in fakeData:
            # pick up a server randomly
            ret.append(
                (f,clusterTest.clusterTestServerList[
                    random.randint(0,len(clusterTest.clusterTestServerList)-1)]))
        return ret

    def _dumpFailedRequest(self,data,methodName):
        f = open("%s.failed.%.json"%(methodName,threading.active_count()),"w")
        f.write(data)
        f.close()

    def _sendRequest(self,methodName,d,server):
        req = urllib2.Request("http://%s/ec2/%s" % (server,methodName), \
            data=d, headers={'Content-Type': 'application/json'})
        response = urllib2.urlopen(req)

        # Do a sligtly graceful way to dump the sample of failure 
        if response.getcode() != 200:
            self._dumpFailedRequest(d,methodName)

        self.assertTrue(response.getcode() == 200, \
            "%s failed with statusCode %d" % (methodName,response.getcode()))
        print "%s OK\r\n" % (methodName)

    def test(self):
        postDataList = self._generateModifySeq()

        # skip this class
        if postDataList == None:
            return

        workerQueue=ClusterWorker(32,len(postDataList))

        print "===================================\n"
        print "Test:%s start!\n"%(self._getMethodName())

        for test in postDataList:
            workerQueue.enqueue( self._sendRequest , (self._getMethodName(),test[0],test[1],) )

        workerQueue.join()
                
        time.sleep(clusterTest.clusterTestSleepTime)
        ret , reason = clusterTest.checkMethodStatusConsistent(self._getObserverName())
        self.assertTrue(ret,reason)

        print "Test:%s finish!\n"%(self._getMethodName())
        print "===================================\n"


class CameraTest():
    _gen=None
    _testCase=2

    def setTestCase(self,num):
        self._testCase=num

    def setUp(self):
        self._gen = CameraDataGenerator()
    
    def _generateModifySeq(self):
        return self._defaultModifySeq(
               self._gen.generateCameraData(
               self._testCase))

    def _getMethodName(self):
        return "saveCameras"

    def _getObserverName(self):
        return "getCameras?format=json"


class UserTest():
    _gen = None
    _testCase=2

    def setTestCase(self,num):
        self._testCase=num

    def setUp(self):
        self._gen = UserDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
               self._gen.generateUserData(
               self._testCase))

    def _getMethodName(self):
        return "saveUser"

    def _getObserverName(self):
        return "getUsers?format=json"


class MediaServerTest():
    _gen=None
    _testCase=2

    def setTestCase(self,num):
        self._testCase=num

    def setUp(self):
        self._gen = MediaServerGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
               self._gen.generateMediaServerData(
               self._testCase))

    def _getMethodName(self):
        return "saveMediaServer"

    def _getObserverName(self):
        return "getMediaServersEx?format=json"

class ResourceSaveTest(ClusterTestBase):
    _gen = None
    _testCase =2

    def setTestCase(self,num):
        self._testCase=num

    def setUp(self):
        self._gen = ResourceDataGenerator()
    
    def _generateModifySeq(self):
        return self._defaultModifySeq(
                self._gen.generateSaveResource(
                self._testCase))

    def _getMethodName(self):
        return "saveResource"

    def _getObserverName(self):
        return "getResourceTypes?format=json"


class ResourceParaTest():
    _gen = None
    _testCase=2

    def setTestCase(self,num):
        self._testCase=num

    def setUp(self):
        self._gen = ResourceDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
               self._gen.generateResourceParams(
               self._testCase))
    
    def _getMethodName(self):
        return "setResourceParams"

    def _getObserverName(self):
        return "getResourceParams?format=json"


class ResourceRemoveTest():
    _gen = None
    _testCase=2

    def setTestCase(self,num):
        self._testCase=num

    def setUp(self):
        self._gen = ResourceDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
                self._gen.generateRemoveResource(
                self._testCase))

    def _getMethodName(self):
        return "removeResource"

    def _getObserverName(self):
        return "getResourceTypes?format=json"


class CameraUserAttributeListTest():
    _gen = None
    _testCase=2

    def setTestCase(self,num):
        self._testCase=num

    def setUp(self):
        self._gen = CameraUserAttributesListDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
                self._gen.generateCameraUserAttribute(
                self._testCase))

    def _getMethodName(self):
        return "saveCameraUserAttributesList"

    def _getObserverName(self):
        return "getCameraUserAttributes"


class ServerUserAttributesListDataTest():
    _gen = None
    _testCase=2

    def setTestCase(self,num):
        self._testCase=num

    def setUp(self):
        self._gen = ServerUserAttributesListDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
                self._gen.generateServerUserAttributesList(self._testCase))

    def _getMethodName(self):
        return "saveServerUserAttributesList"

    def _getObserverName(self):
        return "getServerUserAttributes"

# The following test will issue the modify and remove on different servers to
# trigger confliction resolving.
class ResourceConflictionTest():
    _gen = None
    _testCase=2

    def setTestCase(self,num):
        self._testCase=num

    def setUp(self):
        self._gen = ResourceDataGenerator()

    def _generateRandomServerPair(self):
        # generate first server here
        s1 = clusterTest.clusterTestServerList[
            random.randint(0,len(clusterTest.clusterTestServerList)-1)]
        s2 = None
        if len(clusterTest.clusterTestServerList) == 1:
            s2 = s1
        else:
            while True:
                s2 = clusterTest.clusterTestServerList[
                random.randint(0,len(clusterTest.clusterTestServerList)-1)]
                if s2 != s1:
                    break
        return (s1,s2)

    # Overwrite the test function since the base method doesn't work here
    def test(self):
        postDataList = self._gen.generateResourceConfliction(self._testCase)

        workerQueue=ClusterWorker(32,len(postDataList)*2)

        print "===================================\n"
        print "Test:Resource Confliction start!\n"

        for test in postDataList:
            s1,s2 = self._generateRandomServerPair()
            # modify the resource
            workerQueue.enqueue( self._sendRequest , ("saveResource",test[0],s1,) )
            # remove the resource
            workerQueue.enqueue( self._sendRequest , ("removeResource",test[1],s2) )

        workerQueue.join()
                
        time.sleep(clusterTest.clusterTestSleepTime)
        ret , reason = clusterTest.checkMethodStatusConsistent( \
            "getResourceTypes?format=json")

        self.assertTrue(ret,reason)

        print "Test:Resource Confliction finish!\n"
        print "===================================\n"


# Performance test function
# only support add/remove ,value can only be user and media server
class PerformanceOperation():
    def _sendRequest(self,methodName,d,server):
        req = urllib2.Request("http://%s/ec2/%s" % (server,methodName), \
        data=d, headers={'Content-Type': 'application/json'})
        response = urllib2.urlopen(req)

        # Do a sligtly graceful way to dump the sample of failure 
        if response.getcode() != 200:
            self._dumpFailedRequest(d,methodName)

        if response.getcode() != 200:
            print "%s failed with statusCode %d" % (methodName,response.getcode())
        else:
            print "%s OK\r\n" % (methodName)


    def _getUUIDList(self,methodName):
        response = urllib2.urlopen(
            "http://%s/ec2/%s?format=json"%(
                clusterTest.clusterTestServerList[0],methodName))

        if response.getcode() != 200:
            return None

        json_obj = json.loads(response.read())
        ret=[]
        for entry in json_obj:
            ret.append(entry["id"])
        return ret

    def _sendOp(self,methodName,dataList):
        worker = ClusterWorker(32,len(dataList))
        for d in dataList:
            worker.enqueue(
                self._sendRequest,
                (methodName,d,
                    clusterTest.clusterTestServerList[
                        random.randint(0,len(clusterTest.clusterTestServerList)-1)]))

        worker.join()

    _resourceRemoveTemplate = """
        {
            "id":"%s"
        }
    """
    def _removeAll(self,uuidList):
        data=[]
        for uuid in uuidList:
            data.append(
                self._resourceRemoveTemplate%(uuid))
        self._sendOp("removeResource",data)

    def _remove(self,uuid):
        self._removeAll([uuid])

class UserOperation(PerformanceOperation):
    def add(self,num):
        gen = UserDataGenerator()
        self._sendOp("saveUser",gen.generateUserData(num))
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
        self._sendOp("saveMediaServer",
                     gen.generateMediaServerData(num))
        return True

    def remove(self,uuid):
        self_.remove(uuid)
        return True

    def removeAll(self):
        uuidList=self._getUUIDList("getMediaServersEx?format=json")
        if uuidList == None:
            return False
        self._removeAll(uuidList)
        return True

class CameraOperation(PerformanceOperation):
    def add(self,num):
        gen = CameraDataGenerator()
        self._sendOp(
            "saveCameras",
            gen.generateCameraData(num))
        return True

    def remove(self,uuid):
        self._remove(uuid)
        return True

    def removeAll(self):
        uuidList=self._getUUIDList("getCameras?format=json")
        if uuidList == None:
            return False
        self._removeAll(uuidList)
        return True

def runPerformanceTest():
    if len(sys.argv) != 3 and len(sys.argv) != 2 :
        return (False,"2 or 1 parameters are needed")

    l = sys.argv[1].split('=')
    
    if l[0] != '--add' and l[0] != '--remove':
        return (False,"Unknown first parameter options")

    t=globals()["%sOperation"%(l[1])]

    if t == None:
        return (False,"Unknown target operations:%s"%(l[1]))
    else:
        t = t()
    
    if l[0] == '--add':
        if len(sys.argv) != 3 :
            return (False,"--add must have --count option")
        l = sys.argv[2].split('=')
        if l[0] == '--count':
            num=int(l[1])
            if num <=0 :
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
        return (False,"Unknown command:%s"%(l[0]))

    return True
            
if __name__ == '__main__':
    # initialize cluster test environment
    ret,reason = clusterTest.init()

    if ret == False:
        print "Failed to initialize the cluster test object:%s"%(reason)
    else:
        if len(sys.argv) == 1:
            unittest.main()
        else:
            ret = runPerformanceTest()
            if ret != True:
                print ret[1]
