# -*- coding: utf-8 -*-
__author__ = 'Danil Lavrentyuk'
"""The generator utility classes for functional tests.
(Only those simple generators which don't depend on testMaster object data etc.)
"""
import json, random, string
from hashlib import md5
import threading
from functest_util import ClusterWorker, sendRequest, TestRequestError
from testbase import getTestMaster
from pycommons.GenData import BasicGenerator

__all__ = [
    'BasicGenerator', 'UserDataGenerator', 'MediaServerGenerator',  'CameraConflictionDataGenerator',
    'UserConflictionDataGenerator', 'MediaServerConflictionDataGenerator',
    'CameraDataGenerator', 'ConflictionDataGenerator',
    'ResourceDataGenerator',
    'CameraUserAttributesListDataGenerator',
    'ServerUserAttributesListDataGenerator',
]


class UserDataGenerator(BasicGenerator):
    _template = """
    {
        "digest": "%s",
        "email": "%s",
        "hash": "%s",
        "id": "%s",
        "isAdmin": %s,
        "name": "%s",
        "parentId": "",
        "permissions": "%s",
        "userRoleId": "%s",
        "typeId": "{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}",
        "url": ""
    }
    """
    #         "parentId": "{00000000-0000-0000-0000-000000000000}",

    # about permissions see common/src/common/common_globals.h, GlobalPermission enum

    def generateUserData(self, number, permissions="GlobalAdminPermission", role=''):
        ret = []
        for i in xrange(number):
            id = self.generateRandomId()
            un,pwd,digest = self.generateUsernamePasswordAndDigest(self.generateUserName)
            ret.append((self._template % (digest,
                    self.generateEmail(),
                    self.generatePasswordHash(pwd),
                    id, "false", un,
                    permissions if role == '' else '', role),
                id))

        return ret

    def generateUpdateData(self, id):
        un,pwd,digest = self.generateUsernamePasswordAndDigest(self.generateUserName)
        return (self._template % (digest,
                self.generateEmail(),
                self.generatePasswordHash(pwd),
                id, "false", un, self._permissions), id)

    def createManualUpdateData(self, id, username, password, admin, email, perm=None):
        if perm is None:
            perm = self._permissions
        digest = self.generateDigest(username, password)
        hash = self.generatePasswordHash(password)
        return self._template % (
            digest, email, hash, id, "true" if admin else "false", username, perm)


class MediaServerGenerator(BasicGenerator):
    _serverVersion = "3.0.0.0"
    _template = """
    {
        "apiUrl": "%s",
        "url": "rtsp://%s",
        "authKey": "%s",
        "flags": "SF_HasPublicIP",
        "id": "%s",
        "name": "%s",
        "networkAddresses": "%s",
        "panicMode": "PM_None",
        "parentId": "{00000000-0000-0000-0000-000000000000}",
        "systemInfo": "windows x64 win78",
        "systemName": "%s",
        "typeId": "{be5d1ee0-b92c-3b34-86d9-bca2dab7826f}",
        "version": "%s"
    }
    """

    def _genServerIPs(self):
        return "192.168.0.1;10.0.2.141;95.31.23.214"

    def generateMediaServerData(self, number):

        return [ (self._template % (addr, addr,
                   self.generateRandomId(),
                   id,
                   self.generateMediaServerName(),
                   ';'.join((addr.split(':')[0], self._genServerIPs())),
                   self.generateRandomString(random.randint(5,20)),
                   self._serverVersion),
                id)
                for id, addr in (
                     (self.generateRandomId(), self.generateIpV4Endpoint()) for _ in xrange(number))
        ]

# This class is used to generate data for simulating resource confliction

# Currently we don't have general method to _UPDATE_ an existed record in db,
# so I implement it through creation routine since this routine is the only way
# to modify the existed record now.
class CameraConflictionDataGenerator(BasicGenerator):
    #(id,mac,model,parentId,typeId,url,vendor)
    _existedCameraList = []
    # For simplicity , we just modify the name of this camera
    _updateMetaTemplate = """
        [{
            "groupId": "",
            "groupName": "",
            "id": "%%s",
            "mac": "%%s",
            "manuallyAdded": false,
            "maxArchiveDays": 0,
            "minArchiveDays": 0,
            "model": "%%s",
            "motionMask": "",
            "motionType": "MT_Default",
            "name": "%%s",
            "parentId": "%%s",
            "physicalId": "%%s",
            "%s": "{00000000-0000-0000-0000-000000000000}",
            "scheduleEnabled": false,
            "scheduleTasks": [ ],
            "secondaryStreamQuality": "SSQualityLow",
            "status": "Unauthorized",
            "statusFlags": "CSF_NoFlags",
            "typeId": "%%s",
            "url": "%%s",
            "vendor": "%%s"
        }]
    """
    _updateTemplate = None

    def __init__(self, dataGen):
        if self._updateTemplate is None:
            type(self)._updateTemplate = self._updateMetaTemplate % (
                testMaster.api.preferredServerId)
        if self._fetchExistedCameras(dataGen) == False:
            raise Exception("Cannot get existed camera list")

    def _fetchExistedCameras(self, dataGen):
        for entry in dataGen.conflictCameraList:
            obj = json.loads(entry)[0]
            self._existedCameraList.append((obj["id"],obj["mac"],obj["model"],obj["parentId"],
                 obj["typeId"],obj["url"],obj["vendor"]))
        return True

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
        return self._updateTemplate % (user[0], user[1], user[2], user[3], name, user[4])

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

    _existedMediaServerList = []

    def _fetchExistedMediaServer(self, dataGen):
        for server in dataGen.conflictMediaServerList:
            obj = json.loads(server)
            self._existedMediaServerList.append((obj["apiUrl"],obj["authKey"],obj["id"],
                                                 obj["systemName"],obj["url"]))
        return bool(self._existedMediaServerList)

    def __init__(self, dataGen):
        if not self._fetchExistedMediaServer(dataGen):
            raise Exception("Cannot fetch media server list")

    def _generateModify(self, server):
        name = self.generateRandomString(random.randint(8,20))
        return self._updateTemplate % (server[0],server[1],server[2],name,
            server[3],server[4])

    def _generateRemove(self, server):
        return self._removeTemplate % (server[2])

    def generateData(self):
        server = random.choice(self._existedMediaServerList)
        return (self._generateModify(server),self._generateRemove(server))

####################################################################################################
## Some more generators, a bit more complex. They depend on testMaster
####################################################################################################

testMaster = getTestMaster()

def dataUpdateTask(lock, collector, data, server, method):
    url = "http://%s/ec2/%s" % (server, method)
    sendRequest(lock, url, data[0])  # it raises TestRequestError if not ok
    testMaster.unittestRollback.addOperations(method, server, data[1])
    collector.append(data[0])


class CameraDataGenerator(BasicGenerator):
    # depends on testMaster.clusterTestServerUUIDList and testMaster.clusterTestServerList
    _metaTemplate = """[
        {
            "audioEnabled": %%s,
            "controlEnabled": %%s,
            "dewarpingParams": "",
            "groupId": "",
            "groupName": "",
            "id": "%%s",
            "mac": "%%s",
            "manuallyAdded": false,
            "maxArchiveDays": 0,
            "minArchiveDays": 0,
            "model": "%%s",
            "motionMask": "",
            "motionType": "MT_Default",
            "name": "%%s",
            "parentId": "%%s",
            "physicalId": "%%s",
            "%s": "{00000000-0000-0000-0000-000000000000}",
            "scheduleEnabled": false,
            "scheduleTasks": [ ],
            "secondaryStreamQuality": "SSQualityLow",
            "status": "Unauthorized",
            "statusFlags": "CSF_NoFlags",
            "typeId": "{7d2af20d-04f2-149f-ef37-ad585281e3b7}",
            "url": "%%s",
            "vendor": "%%s"
        }
    ]"""
    _template = None

    def __init__(self):
        super(CameraDataGenerator, self).__init__()
        if self._template is None:
            type(self)._template = self._metaTemplate % (testMaster.api.preferredServerId)


    def _generateCameraId(self,mac):
        return self.generateUUIdFromMd5(mac)

    def _getServerUUID(self, addr):
        if addr == None:
            return "{2571646a-7313-4324-8308-c3523825e639}"
        i = testMaster.clusterTestServerList.index(addr)  # it really has to be there
        return testMaster.clusterTestServerUUIDList[i][0]

    def generateCameraData(self, number, mediaServer):
        return [
            self.generateUpdateData(None, mediaServer, None) for _ in xrange(number)
        ]

    def generateUpdateData(self, id, mediaServer, mac=None):
        if mac is None:
            mac = self.generateMac()
        if id is None:
            id = self._generateCameraId(mac)
        name_and_model = self.generateCameraName()
        return (self._template % (self.generateBool(),
                self.generateBool(),
                id,
                mac,
                name_and_model,
                name_and_model,
                self._getServerUUID(mediaServer),
                mac,
                self.generateIpV4(),
                self.generateRandomString(4)),
            id)


# This class serves as an in-memory data base.
# Before doing the confliction test, we create some sort of resources and then start doing the
# confliction test.
# These data is maintained in a separate dictionary and when everything is done it will be rolled back.
class ConflictionDataGenerator(BasicGenerator):
    conflictCameraList = []
    conflictUserList = []
    conflictMediaServerList = []
    _lock = threading.Lock()

    def _prepareData(self, op, num, methodName, objList):
        worker = ClusterWorker(8, len(testMaster.clusterTestServerList) * num, doStart=True)
        for _ in xrange(num):
            for srv in testMaster.clusterTestServerList:
                worker.enqueue(dataUpdateTask, (self._lock, objList, op(1)[0], srv, methodName))
        worker.join()
        return True

    def _prepareCameraData(self, op, num, methodName, objList):
        worker = ClusterWorker(8, len(testMaster.clusterTestServerList) * num)
        for _ in xrange(num):
            for srv in testMaster.clusterTestServerList:
                worker.enqueue(dataUpdateTask, (self._lock, objList, op(1, srv)[0], srv, methodName))
        worker.join()
        return True

    def prepare(self,num):
        return (
            self._prepareCameraData(CameraDataGenerator().generateCameraData, num, "saveCameras", self.conflictCameraList) and
            self._prepareData(UserDataGenerator().generateUserData, num, "saveUser", self.conflictUserList) and
            self._prepareData(MediaServerGenerator().generateMediaServerData, num, "saveMediaServer", self.conflictMediaServerList)
        )


class ResourceDataGenerator(BasicGenerator):

    _ec2ResourceGetter = ["getCameras", "getMediaServersEx", "getUsers"]

    _resourceParEntryTemplate = """
        {
            "name":"%s",
            "resourceId":"%s",
            "value":"%s"
        }
    """

    _resourceRemoveTemplate = '{ "id":"%s" }'

    # this list contains all the existed resource that I can find.
    # The method for finding each resource is based on the API in
    # the list _ec2ResourceGetter. What's more, the parentId, typeId
    # and resource name will be recorded (Can be None).
    _existedResourceList = []

    def __init__(self,num):
        ret = self._retrieveResourceUUIDList(num)
        if ret == False:
            raise Exception("cannot retrieve resources list on server side")

    def _retrieveResourceUUIDList(self,num):
        """ Retrieves the resource list on the server side.
            Retrieves only the very first server's resource list since each server has exactly the same resources
        """
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

    def _generateKeyValue(self,uuid):
        key_len = random.randint(4,12)
        val_len = random.randint(24,512)
        return self._resourceParEntryTemplate % (self.generateRandomString(key_len),
            uuid,
            self.generateRandomString(val_len))

    def _getRandomResourceUUID(self):
        #FIXME: collisions possible!
        return random.choice(self._existedResourceList)[0]

    def _generateOneResourceParams(self):
        uuid = self._getRandomResourceUUID()
        return "[" + ",".join([
            self._generateKeyValue(uuid) for _ in xrange(random.randint(2,20))
        ])  + "]"

    def generateResourceParams(self, number):
        return [self._generateOneResourceParams() for _ in xrange(number)]

    def generateRemoveResourceIds(self, number):
        return [res[0] for res in random.sample(self._existedResourceList, number)]

    def generateRemoveResource(self, resId):
        return self._resourceRemoveTemplate % resId


class CameraUserAttributesListDataGenerator(BasicGenerator):
    _metaTemplate = """
        [
            {
                "audioEnabled": %%s,
                "%s": "%%s",
                "cameraName": "%%s",
                "controlEnabled": %%s,
                "dewarpingParams": "%%s",
                "maxArchiveDays": -30,
                "minArchiveDays": -1,
                "motionMask": "5,0,0,44,32:5,0,0,44,32:5,0,0,44,32:5,0,0,44,32",
                "motionType": "MT_SoftwareGrid",
                "%s": "%%s",
                "scheduleEnabled": false,
                "scheduleTasks": [ ],
                "secondaryStreamQuality": "SSQualityMedium"
            }
        ]
        """
    _template = None

    _dewarpingTemplate = '''{ \\"enabled\\":%s, \\"fovRot\\":%s,
            \\"hStretch\\":%s, \\"radius\\":%s, \\"viewMode\\":\\"VerticalDown\\",
            \\"xCenter\\":%s,\\"yCenter\\":%s}'''

    _existedCameraUUIDList = []

    _lock = threading.Lock()

    def __init__(self, prepareNum):
        if self._template is None:
            type(self)._template = self._metaTemplate % (
                testMaster.api.cameraId, testMaster.api.preferredServerId)
        if self._fetchExistedCameraUUIDList(prepareNum) == False:
            raise Exception("Cannot initialize camera list attribute test data")

    def _prepareData(self, func, num, methodName, collector):
        worker = ClusterWorker(8, num * len(testMaster.clusterTestServerList), doStart=True)

        for _ in xrange(num):
            for srv in testMaster.clusterTestServerList:
                worker.enqueue(dataUpdateTask, (self._lock, collector, func(1, srv)[0], srv, methodName))

        worker.join()
        return True

    def _fetchExistedCameraUUIDList(self, num):
        # We could not use existed camera list since if we do so we may break
        # the existed
        # database on the server side.  What we gonna do is just create new
        # fake cameras and
        # then do the test by so.
        json_list = []

        if not self._prepareData(CameraDataGenerator().generateCameraData, num, "saveCameras", json_list):
            return False

        for entry in json_list:
            obj = json.loads(entry)[0]
            self._existedCameraUUIDList.append((obj["id"],obj["name"]))

        return True

    def _getRandomServerId(self):
        idx = random.randint(0, len(testMaster.clusterTestServerUUIDList) - 1)
        return testMaster.clusterTestServerUUIDList[idx][0]

    def _generateNormalizeRange(self):
        return str(random.random())

    def _generateDewarpingPar(self):
        return self._dewarpingTemplate % (self.generateBool(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange(),
            self._generateNormalizeRange())

    def _getRandomCameraUUIDAndName(self):
        return random.choice(self._existedCameraUUIDList)

    def generateCameraUserAttribute(self,number):
        return [ self._template % (self.generateBool(),
            uuid, name,
            self.generateBool(),
            self._generateDewarpingPar(),
            self._getRandomServerId()) for uuid , name in (
                self._getRandomCameraUUIDAndName() for _ in xrange(number)
            )
        ]


class ServerUserAttributesListDataGenerator(BasicGenerator):
    _metaTemplate = """
    [
        {
            "allowAutoRedundancy": %%s,
            "maxCameras": %%s,
            "%s": "%%s",
            "serverName": "%%s"
        }
    ]
    """
    _template = None

    _existedFakeServerList = []

    _lock = threading.Lock()

    def _prepareData(self, dataList, methodName, jsonList):
        worker = ClusterWorker(8, len(dataList) * len(testMaster.clusterTestServerList), doStart=True)

        for data in dataList:
            for srv in testMaster.clusterTestServerList:
                worker.enqueue(dataUpdateTask, (self._lock, jsonList, data, srv, methodName,))

        worker.join()
        return True

    def _generateFakeServer(self,num):
        json_list = []

        if not self._prepareData(MediaServerGenerator().generateMediaServerData(num), "saveMediaServer", json_list):
            return False

        for entry in json_list:
            obj = json.loads(entry)
            self._existedFakeServerList.append((obj["id"],obj["name"]))

        return True

    def __init__(self,num):
        if self._template is None:
            type(self)._template = self._metaTemplate % (testMaster.api.serverId,)
        if not self._generateFakeServer(num) :
            raise Exception("Cannot initialize server list attribute test data")

    def _getRandomServer(self):
        if not self._existedFakeServerList:
            raise Exception("Empty _existedFakeServerList")
        return random.choice(self._existedFakeServerList)

    def generateServerUserAttributesList(self,number):
        ret = []
        for i in xrange(number):
            uuid, name = self._getRandomServer()
            ret.append(self._template % (self.generateBool(),
                    random.randint(0,200),
                    uuid,name))
        return ret

####################################################################################################
