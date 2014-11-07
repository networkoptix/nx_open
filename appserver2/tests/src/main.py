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

#class TestSequenceFunctions(unittest.TestCase):

#    def setUp(self):
#        self.seq = range(10)

#    def test_shuffle(self):
#        # make sure the shuffled sequence does not lose any elements
#        random.shuffle(self.seq)
#        self.seq.sort()
#        self.assertEqual(self.seq, range(10))

#        # should raise an exception for an immutable sequence
#        self.assertRaises(TypeError, random.shuffle, (1,2,3))

#    def test_choice(self):
#        element = random.choice(self.seq)
#        self.assertTrue(element in self.seq)

#    def test_sample(self):
#        with self.assertRaises(ValueError):
#            random.sample(self.seq, 20)
#        for element in random.sample(self.seq, 5):
#            self.assertTrue(element in self.seq)

ec2GetRequests = [
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

serverAddress = None
serverPort = None

class EnumerateAllEC2GetCallsTest():
    def test_callAllGetters(self):
        for reqName in ec2GetRequests:
            response = urllib2.urlopen("http://%s:%d/ec2/%s" % (serverAddress,serverPort,reqName))
            self.assertTrue(response.getcode() == 200, "%s failed with statusCode %d" % (reqName,response.getcode()))
            print "%s OK\r\n" % (reqName)


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

    def _getServerUUIDAndName(self,ip,json_obj):
        for server in json_obj:
            ip_list = server["networkAddresses"].split(";")
            for this_ip in ip_list:
                if this_ip == ip:
                    return (server["id"],server["name"])
        return None

    # call this function after reading the clusterTestServerList
    def _fetchClusterTestServerUUIDList(self):
        response = urllib2.urlopen(
            "http://%s/ec2/getMediaServersEx?format=json"%(self.clusterTestServerList[0]))

        if response.getcode() != 200:
            return False

        json_obj = json.loads( response.read() )

        for s in self.clusterTestServerList:
            ip = s.split(":")[0]
            uuid_and_name = self._getServerUUIDAndName(ip,json_obj)
            if uuid_and_name == None:
                return False
            self.clusterTestServerUUIDList.append(uuid_and_name)

        return True

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
            ret,reason = self._checkResultEqual(responseList,method)

            if ret == False:
                print "Failed:%s"%(reason)

            return ret

    def _ensureServerListStates(self,sleep_timeout):
        time.sleep(sleep_timeout)
        for method in self._getterAPIList:
            if self.checkMethodStatusConsistent(method) == False:
                return False;
        return True

    def _loadConfig(self):
        parser = ConfigParser.RawConfigParser();
        parser.read("ec2_tests.cfg")
        self.clusterTestServerList = parser.get("ClusterTest","serverList").split(",")
        self.clusterTestSleepTime = parser.getint("ClusterTest","clusterTestSleepTime")

    def init(self):
        self._loadConfig()
        # configure different domain password strategy
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for s in self.clusterTestServerList:
            passman.add_password( "NetworkOptix", 
                                  "http://%s/ec2/" %(s), serverUser, serverPassword )
        # add default password strategy
        passman.add_password( "NetworkOptix", "http://%s:%d/ec2/" % (serverAddress, serverPort), serverUser, serverPassword )
        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

        # ensure all the server are on the same page
        if self._ensureServerListStates(self.clusterTestSleepTime) == False:
            return False

        if self._fetchClusterTestServerUUIDList() == False:
            return False

clusterTest=ClusterTest()

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
            "typeId": "%s",
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
                    id,
                    self.generateUUIdFromMd5(self.generateRandomString(12)),
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
        "id": "{47bf37a0-72a6-2890-b967-5da9c390d28a}",
        "name": "%s",
        "networkAddresses": "192.168.0.1;10.0.2.141;192.168.88.1;95.31.23.214",
        "panicMode": "PM_None",
        "parentId": "{00000000-0000-0000-0000-000000000000}",
        "systemInfo": "windows x64 win78",
        "systemName": "%s",
        "typeId": "{be5d1ee0-b92c-3b34-86d9-bca2dab7826f}",
        "url": "rtsp:%s",
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
                   self._generateRandomName(),
                   self._generateRandomName(),
                   self.generateIpV4Endpoint()))

        return ret

class ResourceDataGenerator(BasicGenerator):

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

    _existedResourceUUIDList=[]

    _savableResourceList=[]

    # this function is used to retrieve the resource list on the 
    # server side. We just retrieve the resource list from the 
    # very first server since each server has exact same resource

    def _retrieveResourceUUIDList(self):
        response = urllib2.urlopen(
            "http://%s/ec2/getResourceTypes?format=json"%(clusterTest.clusterTestServerList[0]))

        if response == None or response.getcode() != 200:
            return False

        ret = json.loads(response.read())
        
        for ele in ret:
            typeId = None
            parentId=None

            if "propertyTypes" in ele:
                pt = ele["propertyTypes"]
                if pt:
                    typeId = pt[0]["resourceTypeId"]

            if "parentId" in ele:
                pid = ele["parentId"]
                if pid:
                    parentId = pid[0]

            self._existedResourceUUIDList.append( ele["id"] )

            if parentId != None and typeId != None:
                self._savableResourceList.append((ele["id"],parentId,typeId))

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

    def _getRandomExistedUUID(self):
        idx = random.randint(0,len(self._existedResourceUUIDList)-1)
        return self._existedResourceUUIDList[idx]

    def _generateOneResourceParams(self):
        uuid = self._getRandomExistedUUID()
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

    def _getRandomServerId(self):
        idx = random.randint(0,len(clusterTest.clusterTestServerUUIDList)-1)
        return clusterTest.clusterTestServerUUIDList[idx][0]
        
    def generateSaveResource(self,number):
        ret = []
        # get one existed resource from the server 
        res = self._savableResourceList[
            random.randint(0,len(self._savableResourceList)-1)]

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
            ret.append(
                self._resourceRemoveTemplate%(
                    self._getRandomServerId()))
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
        req = urllib2.Request("http://%s/ec2/%s" % (server,methodName), data=d, headers={'Content-Type': 'application/json'})
        response = urllib2.urlopen(req)

        # Do a sligtly graceful way to dump the sample of failure 
        if response.getcode() != 200:
            self._dumpFailedRequest(d,methodName)

        self.assertTrue(response.getcode() == 200, "%s failed with statusCode %d" % (methodName,response.getcode()))
        print "%s OK\r\n" % (methodName)

    def test(self):
        postDataList = self._generateModifySeq()

        # skip this class
        if postDataList == None:
            return

        thread_list = []

        print "===================================\n"
        print "Test:%s start!\n"%(self._getMethodName())

        for test in postDataList:
            t = threading.Thread(target=self._sendRequest,args=(self._getMethodName(),test[0],test[1]))
            thread_list.append( t )
            t.start()

        for t in thread_list:
            t.join()

        time.sleep(clusterTest.clusterTestSleepTime)
        clusterTest.checkMethodStatusConsistent(self._getObserverName())

        print "Test:%s finish!\n"%(self._getMethodName())
        print "===================================\n"


class CameraTest(ClusterTestBase):
    _gen=None
    _cameraTestSize=1

    def setUp(self):
        self._gen = CameraDataGenerator()
    
    def _generateModifySeq(self):
        return self._defaultModifySeq(
               self._gen.generateCameraData(
               self._cameraTestSize))

    def _getMethodName(self):
        return "saveCameras"

    def _getObserverName(self):
        return "getCameras?format=json"


class UserTest(ClusterTestBase):
    _gen = None
    _userTestSize = 1

    def setUp(self):
        self._gen = UserDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
               self._gen.generateUserData(
               self._userTestSize))

    def _getMethodName(self):
        return "saveUsers"

    def _getObserverName(self):
        return "getUsers?format=json"


class MediaServerTest(ClusterTestBase):
    _gen=None
    _mediaServerTestSize=1

    def setUp(self):
        self._gen = MediaServerGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
               self._gen.generateMediaServerData(
               self._mediaServerTestSize))

    def _getMethodName(self):
        return "saveUsers"

    def _getObserverName(self):
        return "getUsers?format=json"

class ResourceSaveTest(ClusterTestBase):
    _gen = None
    _resourceSaveTestSize =1

    def setUp(self):
        self._gen = ResourceDataGenerator()
    
    def _generateModifySeq(self):
        return self._defaultModifySeq(
                self._gen.generateSaveResource(
                self._resourceSaveTestSize))

    def _getMethodName(self):
        return "saveResource"

    def _getObserverName(self):
        return "getResourceTypes?format=json"


class ResourceParaTest(ClusterTestBase):
    _gen = None
    _resourceParaTestSize=1

    def setUp(self):
        self._gen = ResourceDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
               self._gen.generateResourceParams(
               self._resourceParaTestSize))
    
    def _getMethodName(self):
        return "setResourceParams"

    def _getObserverName(self):
        return "getResourceParams?format=json"


class ResourceRemoveTest(ClusterTestBase):
    _gen = None
    _resourceRemoveTestSize=1

    def setUp(self):
        self._gen = ResourceDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
                self._gen.generateRemoveResource(
                self._resourceRemoveTestSize))

    def _getMethodName(self):
        return "removeResource"

    def _getObserverName(self):
        return "getResourceTypes?format=json"


class CameraUserAttributeListTest(ClusterTestBase):
    _gen = None
    _cameraUserAttributeListTestSize=1

    def setUp(self):
        self._gen = CameraUserAttributesListDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
                self._gen.generateCameraUserAttribute(
                self._cameraUserAttributeListTestSize))

    def _getMethodName(self):
        return "saveCameraUserAttributesList"

    def _getObserverName(self):
        return "getCameraUserAttributes"


class ServerUserAttributesListDataTest(ClusterTestBase):
    _gen = None
    _serverUserAttributeListTestSize=1

    def setUp(self):
        self._gen = ServerUserAttributesListDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(
                self._gen.generateServerUserAttributesList(
                self._serverUserAttributeListTestSize))

    def _getMethodName(self):
        return "saveServerUserAttributesList"

    def _getObserverName(self):
        return "getServerUserAttributes"

    
if __name__ == '__main__':
    # reading config
    config = ConfigParser.RawConfigParser()
    config.read("ec2_tests.cfg")
    serverAddress = config.get("General", "serverAddress")
    serverPort = config.getint("General", "port")
    serverUser = config.get("General", "user")
    serverPassword = config.get("General", "password")

    # initialize cluster test environment
    if clusterTest.init() == False:
        raise Exception("cannot initialize cluster test object")

    unittest.main()
