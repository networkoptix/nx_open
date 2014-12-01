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

class ClusterTest():
    clusterTestServerList = []
    clusterTestSleepTime = None
    clusterTestServerUUIDList = []
    testCaseSize = 2

    _getterAPIList = ["getResourceTypes",
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
        "getLicenses"]

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

    def _checkResultEqual(self,responseList,methodName):
        result = None
        for response in responseList:
            if response.getcode() != 200:
                return(False,"method:%s http request failed with code:%d" % (methodName,response.getcode))
            else:
                # painfully slow, just bulk string comparison here
                if result == None:
                    result = response.read()
                else:
                    output = response.read()
                    if result != output:
                        return(False,"method:%s return value differs from other" % (methodName))
                
                response.close()

        return (True,"")

    def checkMethodStatusConsistent(self,method):
            responseList = []
            for server in self.clusterTestServerList:
                responseList.append((urllib2.urlopen("http://%s/ec2/%s" % (server, method))))

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
        parser = ConfigParser.RawConfigParser()
        parser.read("ec2_tests.cfg")
        self.clusterTestServerList = parser.get("General","serverList").split(",")
        self.clusterTestSleepTime = parser.getint("General","clusterTestSleepTime")
        try :
            self.testCaseSize = parser.getint("General","testCaseSize")
        except :
            self.testCaseSize = 2

        return (parser.get("General","password"),parser.get("General","username"))

    def init(self):
        pwd,un = self._loadConfig()
        # configure different domain password strategy
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for s in self.clusterTestServerList:
            passman.add_password("NetworkOptix","http://%s/ec2/" % (s), un, pwd)
        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

        # ensure all the server are on the same page
        ret,reason = self._ensureServerListStates(self.clusterTestSleepTime)

        if ret == False:
            return (ret,reason)

        ret,reason = self._fetchClusterTestServerUUIDList()

        if ret == False:
            return (ret,reason)

        return (True,"")

clusterTest = ClusterTest()

class EnumerateAllEC2GetCallsTest():
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

    def test_callAllGetters(self):
        for s in clusterTest.clusterTestServerList:
            for reqName in ec2GetRequests:
                response = urllib2.urlopen("http://%s/ec2/%s" % (s,reqName))
                self.assertTrue(response.getcode() == 200, \
                    "%s failed with statusCode %d" % (reqName,response.getcode()))
            print "Server:%s test for all getter pass" % (s)


class BasicGenerator():
    # generate MAC address of the object
    def generateMac(self):
        l = []
        for i in range(0,5):
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
        return ''.join(random.choice(chars) for _ in range(length))

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

    def generateUsernamePasswordAndDigest(self):
        un_len = random.randint(4,12)
        pwd_len = random.randint(8,20)

        un = self.generateRandomString(un_len)
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
            name_and_model = self.generateRandomString(6)
            ret.append(self._template % (self.generateTrueFalse(),
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
        ret = []
        for i in range(number):
            un,pwd,digest = self.generateUsernamePasswordAndDigest()
            ret.append(self._template % (digest,
                self.generateEmail(),
                self.generatePasswordHash(pwd),
                self.generateRandomId(),
                un))

        return ret

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
        for i in range(number):
            ret.append(self._template % (self.generateIpV4Endpoint(),
                   self.generateRandomId(),
                   self.generateRandomId(),
                   self._generateRandomName(),
                   self._generateRandomName(),
                   self.generateIpV4Endpoint()))

        return ret

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

    def _retrieveResourceUUIDList(self):
        for api in self._ec2ResourceGetter:
            response = urllib2.urlopen("http://%s/ec2/%s?format=json" % (clusterTest.clusterTestServerList[0],
                        api))

            if response.getcode() != 200:
                continue
            json_obj = json.loads(response.read())
            for ele in json_obj:
                self._existedResourceList.append((ele["id"],ele["parentId"],ele["typeId"]))

            response.close()

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
        for i in range(num - 1):
            num = random.randint(2,20)
            kv_list.append(self._generateKeyValue(uuid))
            kv_list.append(",")

        kv_list.append(self._generateKeyValue(uuid))
        kv_list.append("]")

        return ''.join(kv_list)
    
    def generateResourceParams(self,number):
        ret = []
        for i in range(number):
            ret.append(self._generateOneResourceParams())
        return ret

    def generateRemoveResource(self,number):
        ret = []
        for i in range(number):
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
            id="%s"
        }
        """

    def _fetchExistedCameras(self):
        response = urllib2.urlopen("http://%s/ec2/getCameras?format=json" % (clusterTest.clusterTestServerList[0]))

        if response.getcode() != 200 :
            return False

        json_obj = json.loads(response.read())

        for obj in json_obj:
            self._existedCameraList.append((obj["id"],obj["mac"],obj["model"],obj["parentId"],
                 obj["typeId"],obj["url"],obj["vendor"]))

        response.close()

        return True

    def __init__(self):
        if self._fetchExistedCameras() == False:
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
            id="%s"
        }
    """

    _existedUserList = []

    def _fetchExistedUser(self):
        response = urllib2.urlopen("http://%s/ec2/getUsers?format=json" % (clusterTest.clusterTestServerList[0]))

        if response.getcode() != 200:
            return False

        json_obj = json.loads(response.read())

        for entry in json_obj:
            self._existedUserList.append((entry["digest"],entry["email"],entry["hash"],
                 entry["id"],entry["permissions"]))

        response.close()

        return True

    def __init__(self):
        if self._fetchExistedUser() == False:
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
            id="%s"
        }
    """

    _existedMediaServerList = []

    def _fetchExistedMediaServer(self):
        response = urllib2.urlopen("http://%s/ec2/getMediaServersEx?format=json" % (clusterTest.clusterTestServerList[0]))

        if response.getcode() != 200:
            return False

        json_obj = json.loads(response.read())

        for server in json_obj:
            self._existedMediaServerList.append((server["apiUrl"],server["authKey"],server["id"],
                 server["systemName"],server["url"]))

        response.close()

        return True


    def __init__(self):
        if self._fetchExistedMediaServer() == False:
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

    def __init__(self):
        if self._fetchExistedCameraUUIDList() == False:
            raise Exception("cannot fetch camera list on server")

    def _fetchExistedCameraUUIDList(self):
        response = urllib2.urlopen("http://%s/ec2/getCameras?format=json" % (clusterTest.clusterTestServerList[0]))
        if response == None or response.getcode() != 200:
            return False

        ret = json.loads(response.read())

        for ele in ret:
            self._existedCameraUUIDList.append((ele["id"],ele["name"]))

        response.close()

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

        for i in range(number):
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

    def _getRandomServer(self):
        idx = random.randint(0,len(clusterTest.clusterTestServerUUIDList) - 1)
        return clusterTest.clusterTestServerUUIDList[idx]

    def generateServerUserAttributesList(self,number):
        ret = []
        for i in range(number):
            uuid,name = self._getRandomServer()
            ret.append(self._template % (self.generateTrueFalse(),
                    random.randint(0,200),
                    uuid,name))
        return ret



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
        ret = []
        for f in fakeData:
            # pick up a server randomly
            ret.append((f,clusterTest.clusterTestServerList[random.randint(0,len(clusterTest.clusterTestServerList) - 1)]))
        return ret

    def _dumpFailedRequest(self,data,methodName):
        f = open("%s.failed.%.json" % (methodName,threading.active_count()),"w")
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

        response.close()

    def test(self):
        postDataList = self._generateModifySeq()

        # skip this class
        if postDataList == None:
            return

        workerQueue = ClusterWorker(32,len(postDataList))

        print "===================================\n"
        print "Test:%s start!\n" % (self._getMethodName())

        for test in postDataList:
            workerQueue.enqueue(self._sendRequest , (self._getMethodName(),test[0],test[1],))

        workerQueue.join()
                
        time.sleep(clusterTest.clusterTestSleepTime)
        ret , reason = clusterTest.checkMethodStatusConsistent(self._getObserverName())
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
        return self._defaultModifySeq(self._gen.generateCameraData(self._testCase))

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
        return self._defaultModifySeq(self._gen.generateUserData(self._testCase))

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
        return self._defaultModifySeq(self._gen.generateMediaServerData(self._testCase))

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
        self._gen = ResourceDataGenerator()

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
        self._gen = ResourceDataGenerator()

    def _generateModifySeq(self):
        return self._defaultModifySeq(self._gen.generateRemoveResource(self._testCase))

    def _getMethodName(self):
        return "removeResource"

    def _getObserverName(self):
        return "getResourceTypes?format=json"


class CameraUserAttributeListTest(ClusterTestBase):
    _gen = None
    _testCase = clusterTest.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = clusterTest.testCaseSize
        self._gen = CameraUserAttributesListDataGenerator()

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
        self._gen = ServerUserAttributesListDataGenerator()

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
        self._testCase = clusterTest.testCaseSize
        self._conflictList = [("removeResource","saveMediaServer",MediaServerConflictionDataGenerator()),
            ("removeResource","saveUser",UserConflictionDataGenerator()),
            ("removeResource","saveCameras",CameraConflictionDataGenerator())]

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
        workerQueue = ClusterWorker(32,self._testCase * 2)

        print "===================================\n"
        print "Test:ResourceConfliction start!\n"

        for _ in range(self._testCase):
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
    _minData = 2
    _maxData = 8
    _systemNameTemplate = """
    {
        "systemName":"%s"
    }"""

    getterAPI = ["getResourceParams",
        "getMediaServers",
        "getMediaServersEx",
        "getCameras",
        "getUsers",
        "getServerUserAttributes",
        "getCameraUserAttributes"]

    # Function to generate method and class matching
    def _generateDataAndAPIList(self):

        def cameraFunc(num):
            gen = CameraDataGenerator()
            return gen.generateCameraData(num)

        def userFunc(num):
            gen = UserDataGenerator()
            return gen.generateUserData(num)

        def mediaServerFunc(num):
            gen = MediaServerGenerator()
            return gen.generateMediaServerData(num)

        def resourceParAddFunc(num):
            gen = ResourceDataGenerator()
            return gen.generateResourceParams(num)

        def cameraUserAttributesListFunc(num):
            gen = CameraUserAttributesListDataGenerator()
            return gen.generateCameraUserAttribute(num)

        def serverUserAttributesListFunc(num):
            gen = ServerUserAttributesListDataGenerator()
            return gen.generateServerUserAttributesList(num)

        return [("saveCameras",cameraFunc),
            ("saveUser",userFunc),
            ("saveMediaServer",mediaServerFunc),
            ("setResourceParams",resourceParAddFunc),
            ("saveServerUserAttributesList",serverUserAttributesListFunc),
            ("saveCameraUserAttributesList",cameraUserAttributesListFunc)]

    def _sendRequest(self,addr,method,d):
        req = urllib2.Request("http://%s/ec2/%s" % (addr,method), \
              data=d, 
              headers={'Content-Type': 'application/json'})

        response = urllib2.urlopen(req)

        if response.getcode() != 200 :
            return (False,"Cannot issue changeSystemName with HTTP code:%d" % (response.getcode()))

        response.close()

        return (True,"")

    def _setSystemName(self,addr,name):
        return self._sendRequest(addr,"changeSystemName",
                                 self._systemNameTemplate % (name))

    def _generateRandomStates(self,addr):
        api_list = self._generateDataAndAPIList()
        for api in api_list:
            num = random.randint(self._minData,self._maxData)
            data_list = api[1](num)
            for data in data_list:
                ret,reason = self._sendRequest(addr,api[0],data)
                if ret == False:
                    return (ret,reason)
        return (True,"")

    def main(self,addr,name):
        # 1, change the system name
        self._setSystemName(addr,name)
        # 2, generate random data to this server
        ret,reason = self._generateRandomStates(addr)
        if ret == False:
            raise Exception("Cannot generate random states:%s" % (reason))
    
# This class is used to control the whole merge test
class MergeTest():
    _phase1WaitTime = 5
    _systemName = "mergeTest"
    _gen = BasicGenerator()
    _systemNameTemplate = """
    {
        "systemName":"%s"
    }"""

    # This function will ENSURE that the random system name is unique
    def _generateRandomSystemName(self):
        ret = []
        s = set()
        for i in range(len(clusterTest.clusterTestServerList)):
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
        testList = self._generateRandomSystemName()
        worker = ClusterWorker(32,len(testList))

        for entry in testList:
            worker.enqueue(PrepareServerStatus().main,
                (entry[0],entry[1],))

        worker.join()
        time.sleep(self._phase1WaitTime)

    # Second phase will make each server has the same system
    # name which triggers server merge operations there.

    def _setSystemName(self,addr):

        req = urllib2.Request("http://%s/ec2/changeSystemName" % (addr), \
              data=self._systemNameTemplate % (self._systemName), 
              headers={'Content-Type': 'application/json'})

        response = urllib2.urlopen(req)

        if response.getcode() != 200 :
            return (False,"Cannot issue changeSystemName with HTTP code:%d" % (response.getcode()))

        response.close()

        return (True,"")

    def _setToSameSystemName(self):
        worker = ClusterWorker(32,len(clusterTest.clusterTestServerList))
        for entry in  clusterTest.clusterTestServerList:
            worker.enqueue(self._setSystemName,(entry,))
        worker.join()

    def _phase2(self):
        self._setToSameSystemName()
        # Wait until the synchronization time out expires
        time.sleep(clusterTest.clusterTestSleepTime)
        # Do the status checking of _ALL_ API
        for api in PrepareServerStatus.getterAPI:
            ret , reason = clusterTest.checkMethodStatusConsistent("%s?format=json" % (api))
            if ret == False:
                return (ret,reason)
        return (True,"")

    def test(self):
        print "================================\n"
        print "Server Merge Test Start\n"

        self._phase1()
        self._phase2()

        print "Server Merge Test End\n"
        print "================================\n"


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

        response.close()

    def _getUUIDList(self,methodName):
        response = urllib2.urlopen("http://%s/ec2/%s?format=json" % (clusterTest.clusterTestServerList[0],methodName))

        if response.getcode() != 200:
            return None

        json_obj = json.loads(response.read())
        ret = []
        for entry in json_obj:
            ret.append(entry["id"])

        response.close()

        return ret

    def _sendOp(self,methodName,dataList):
        worker = ClusterWorker(32,len(dataList))
        for d in dataList:
            worker.enqueue(self._sendRequest,
                (methodName,d,
                    clusterTest.clusterTestServerList[random.randint(0,len(clusterTest.clusterTestServerList) - 1)]))

        worker.join()

    _resourceRemoveTemplate = """
        {
            "id":"%s"
        }
    """
    def _removeAll(self,uuidList):
        data = []
        for uuid in uuidList:
            data.append(self._resourceRemoveTemplate % (uuid))
        self._sendOp("removeResource",data)

    def _remove(self,uuid):
        self._removeAll([uuid])



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
        worker = ClusterWorker(16 , self._testCase)
        for _ in range(self._testCase):
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
        for i in range(len(clusterTest.clusterTestServerList)):
            serverAddr = clusterTest.clusterTestServerList[i]
            serverAddrGUID = clusterTest.clusterTestServerUUIDList[i][0]
            SingleServerRtspTest(serverAddr,serverAddrGUID,self._testCase,
                              self._username,self._password).run()

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
        uuidList = self._getUUIDList("getMediaServersEx?format=json")
        if uuidList == None:
            return False
        self._removeAll(uuidList)
        return True

class CameraOperation(PerformanceOperation):
    def add(self,num):
        gen = CameraDataGenerator()
        self._sendOp("saveCameras",
            gen.generateCameraData(num))
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


helpStr="Usage:\n" \
    "--merge-test: Test server merge. The user needs to specify more than one server in the config file. \n\n" \
    "--rtsp-test: Test the rtsp streaming. The user needs to specify section [Rtsp] in side of the config file and also " \
    "testSize attribute in it , eg: \n[Rtsp]\ntestSize=100\n Which represent how many test case performed on EACH server.\n\n" \
    "--add=Camera/MediaServer/User --count=num: Add a fake Camera/MediaServer/User to the server you sepcify in the list. The --count " \
    "option MUST be specified , so a working example is like: --add=Camera --count=500 . This will add 500 cameras(fake) into "\
    "the server.\n\n"\
    "--remove=Camera/MediaServer/User (--id=id)*: Remove a Camera/MediaServer/User with id OR remove all the resources.The user can "\
    "specify --id option to enable remove a single resource, eg : --remove=MediaServer --id={SomeGUID} , and --remove=MediaServer will remove " \
    "all the media server.\nNote: --add/--remove will perform the corresponding operations on a random server in the server list if the server list "\
    "have more than one server.\n\n" \
    "If no parameter is specified, the default automatic test will performed, this includes modify the resource and then wait "\
    "for all the server sync their status and also the confliction test as well.The user could specify testCaseSize in configuration file to set the "\
    "test case number for each test class. Eg:\n[General]\ntestCaseSize=5\n, for each test class 5 random cases will be issued.Currently specify large number " \
    "will make the test slow and sometimes cause 401 errors"

            
if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == '--help':
        print helpStr
    else:
        # initialize cluster test environment
        ret,reason = clusterTest.init()
        if ret == False:
            print "Failed to initialize the cluster test object:%s" % (reason)
        else:
            if len(sys.argv) == 1:
                unittest.main()
            else:
                if sys.argv[1] == '--merge-test':
                    MergeTest().test()
                elif sys.argv[1] == '--rtsp-test':
                    ServerRtspTest().test()
                else:
                    ret = runPerformanceTest()
                    if ret != True:
                        print ret[1]
