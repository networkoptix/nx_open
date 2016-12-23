# $Id$
# Artem V. Nikitin
# Single server test sample

import time, os, json, random
from hashlib import md5

from pycommons.MockClient import ClientMixin
from pycommons.SingleServerTest import MediaServerInstanceTest
from pycommons.Logger import LOGLEVEL, log
from pycommons.Config import config
from pycommons.Utils import generateGuid
from pycommons.Rec import Rec
from pycommons.GenData import generateServerData, generateStorageData, generateCameraData, \
     generateServerUserAttributesData, generateCameraUserAttributesData, generateResourceData, \
     generateUserData

class InstanceTest(MediaServerInstanceTest, ClientMixin):
    "Single server instance test"

    helpStr = 'Single server instance test'
    _test_name = 'Single server instance'
    _test_key = 'instance'
    # Test suites
    _suites = ( ('SingeServerInstanceSuits', [
        'testCreateAndCheck',
        'testNonExistUserGroup',
        'testRemoveChildObjects']), )

    def setUp(self):
        MediaServerInstanceTest.setUp(self)
        ClientMixin.setUp(self)
        
    def tearDown(self):
        MediaServerInstanceTest.tearDown(self)
        ClientMixin.tearDown(self)


    # Create user with resource
    def __createUser(self, **kw):
        userData = generateUserData(**kw)
                
        self.sendAndCheckRequest(
           self.server.address, "ec2/saveUser",
           headers={'Content-Type': 'application/json'},
           data=json.dumps(userData.get()))

        userResourceData = generateResourceData(
            resourceId = userData.id)

        self.sendAndCheckRequest(
            self.server.address, "ec2/setResourceParams",
            data=json.dumps([userResourceData.get()]))

        self.__checkPresent("ec2/getUsers",
                            'User', id=userData.id)

        self.__checkPresent("ec2/getResourceParams",
                           'UserResource', resourceId=userData.id)
        return userData

    # Remove user
    def __removeUser(self, userGuid):
        self.sendAndCheckRequest(
            self.server.address, "ec2/removeUser",
            headers={'Content-Type': 'application/json'},
            data=json.dumps({'id': userGuid}))

        self.__checkAbsent("ec2/getUsers",  'User', id = userGuid )
        self.__checkAbsent("ec2/getResourceParams",
                           'UserResource', resourceId=userGuid)
        
    # Check entity (server, camera, user, etc) present
    def __checkPresent(self, __method, __name, **kw):
        response = self.sendAndCheckRequest(
            self.server.address, __method)
        
        self.assertHasItem(kw, response.data, __name)

    # Check entity (server, camera, user, etc) absent
    def __checkAbsent(self, __method, __name, **kw):
        response = self.sendAndCheckRequest(
            self.server.address, __method)
        
        self.assertHasNotItem(kw, response.data, __name)

    # Create camera with attributes & resource
    def __createCamera(self, serverGuid):
        cameraData = generateCameraData(parentId = serverGuid)
        
        self.sendAndCheckRequest(
            self.server.address, "ec2/saveCamera",
            data=json.dumps(cameraData.get()))
        
        cameraUserAttrData = generateCameraUserAttributesData(
            cameraId = cameraData.id)
        
        self.sendAndCheckRequest(
            self.server.address, "ec2/saveCameraUserAttributes",
            data=json.dumps(cameraUserAttrData.get()))
       
        cameraResourceData = generateResourceData(
            resourceId = cameraData.id)

        self.sendAndCheckRequest(
            self.server.address, "ec2/setResourceParams",
            data=json.dumps([cameraResourceData.get()]))

        self.__checkPresent("ec2/getCameras", 'Camera', id=cameraData.id)
        self.__checkPresent("ec2/getCameraUserAttributesList", 'CameraAttributes',
                            cameraId=cameraData.id)
        self.__checkPresent("ec2/getResourceParams",
                            'CameraResource', resourceId=cameraData.id)
        
        return cameraData

    # Check camera & child object was removed
    def __checkCameraRemoved(self, cameraId):
        self.__checkAbsent("ec2/getCameras", 'Camera', id = cameraId)
        self.__checkAbsent("ec2/getCameraUserAttributesList", 'Camera',
                            cameraId=cameraId)
        self.__checkAbsent("ec2/getResourceParams",
                           'ServerResource', resourceId=cameraId)

    # Create storage
    def __createStorage(self, serverGuid):
        storageData = generateStorageData(parentId = serverGuid)

        self.sendAndCheckRequest(
            self.server.address, "ec2/saveStorage",
            data=json.dumps(storageData.get()))

        self.__checkPresent("ec2/getStorages", 'Storage', id=storageData.id)

        return storageData

    # Create server with attributes & resource
    def __createServer(self):
        serverData = generateServerData()

        self.sendAndCheckRequest(
            self.server.address, "ec2/saveMediaServer",
            data=json.dumps(serverData.get()))

        serverUserAttrData = generateServerUserAttributesData(
            serverId = serverData.id)

        self.sendAndCheckRequest(
            self.server.address, "ec2/saveMediaServerUserAttributes",
            data=json.dumps(serverUserAttrData.get()))

        serverResourceData = generateResourceData(
            resourceId = serverData.id)

        self.sendAndCheckRequest(
            self.server.address, "ec2/setResourceParams",
            data=json.dumps([serverResourceData.get()]))

        self.__checkPresent("ec2/getMediaServers", 'Server', id=serverData.id)
        self.__checkPresent("ec2/getMediaServerUserAttributesList",
                              'ServerAttributes', serverId=serverData.id)
        self.__checkPresent("ec2/getResourceParams",
                            'ServerResource', resourceId=serverData.id)
        return serverData

    # https://networkoptix.atlassian.net/browse/VMS-2246
    def testCreateAndCheck(self):
        "Create and check user"
        userData = self.__createUser(
            name = "user1", email = "user1@example.com", permissions = "2432",
            cryptSha512Hash = "", digest = "", hash = "", isAdmin = False,
            isEnabled = True, isLdap = False, realm = "")
        expData = userData.get()
        expData["permissions"] = "GlobalViewArchivePermission|GlobalManageBookmarksPermission|0x80"
        
        # To make sure 
        time.sleep(2.0)
        self.__checkPresent("ec2/getUsers", 'User', **expData)

        self.__removeUser(userData.id)

    # https://networkoptix.atlassian.net/browse/VMS-3052
    def testNonExistUserGroup(self):
        "Unexisting user role"
        unexistsUserRole = generateGuid()
        userData1 = self.__createUser(
            name = "user2", email = "user2@example.com")
        
        userData2 = generateUserData(
            name = "user3", email = "user3@example.com", userRoleId = unexistsUserRole)

        # Try link exists user to unexisting role
        userData1_invalid = Rec(userData1.get())
        userData1_invalid.userRoleId = unexistsUserRole
        self.sendAndCheckRequest(
           self.server.address, "ec2/saveUser",
           status=403,
           data=json.dumps(userData1_invalid.get()))

        # Try create new user to unexisting role
        self.sendAndCheckRequest(
           self.server.address, "ec2/saveUser",
           status=403,
           data=json.dumps(userData2.get()))

        response = self.sendAndCheckRequest(
            self.server.address, "ec2/getUsers")

        self.assertHasItem(userData1.get(), response.data, 'User#1')
        self.assertHasNotItem({'id': userData2.id}, response.data, 'User#2')

        self.__removeUser(userData1.id)

        
    # https://networkoptix.atlassian.net/browse/VMS-2904
    def testRemoveChildObjects(self):
        "Remove child objects"
        # Create server
        serverData = self.__createServer()

        # Create storage
        storageData = self.__createStorage(serverData.id)

        self.__checkPresent("ec2/getStorages", 'Storage', id=storageData.id)

        # Create cameras
        cameraData1 = self.__createCamera(serverData.id)
        cameraData2 = self.__createCamera(serverData.id)

        # Remove camera#1
        self.sendAndCheckRequest(
            self.server.address, "ec2/removeResource",
            data=json.dumps({'id': cameraData1.id}))

        self.__checkCameraRemoved(cameraData1.id)
                
        # Remove server
        self.sendAndCheckRequest(
            self.server.address, "ec2/removeMediaServer",
            data=json.dumps({'id': serverData.id}))

        self.__checkAbsent("ec2/getMediaServers", 'Server', id = serverData.id)
        self.__checkAbsent("ec2/getStorages", 'Storage', id = storageData.id)
        self.__checkAbsent("ec2/getMediaServerUserAttributesList",
                              'ServerAttributes', serverId=serverData.id)
        self.__checkAbsent("ec2/getResourceParams",
                            'ServerResource', resourceId=serverData.id)

        self.__checkCameraRemoved(cameraData2.id)

        
