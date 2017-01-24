# $Id$
# Artem V. Nikitin
# Single server test

import time, os, json, random, urllib
from hashlib import md5
from pycommons.MockClient import ClientMixin, Client, DigestAuthClient
from pycommons.ServerTest import MediaServerInstanceTest
from pycommons.Logger import LOGLEVEL, log
from pycommons.Config import config
from pycommons.Rec import Rec
from pycommons.GenData import generateUserData, BasicGenerator, GeneratorMixin, generateGuid
from pycommons.FillStorage import FillSettings, StorageMixin

class InstanceTest(MediaServerInstanceTest, ClientMixin, StorageMixin, GeneratorMixin):
    "Single server instance test"

    helpStr = 'Single server instance test'
    _test_name = 'Single server instance'
    _test_key = 'instance'
    
    # Test suites
    _suites = ( ('SingeServerInstanceSuits', [
        'testCreateAndCheck',
        'testNonExistUserGroup',
        'testRemoveChildObjects',
        'testServerHeader',
        'testStaticVulnerability',
        'testGetTimePeriods'
        ]), )

    @classmethod
    def prepareEnv(cls, server):
        MediaServerInstanceTest.prepareEnv(server)
        cls.archiveCameraMac = BasicGenerator.generateMac()
        cls.archiveStartTime = time.time()
        cls.expPeriods = server.fillStorage(
            cls.archiveCameraMac,
            FillSettings(startTime = cls.archiveStartTime,
                         step = 1, count = 3))

    def setUp(self):
        MediaServerInstanceTest.setUp(self)
        ClientMixin.setUp(self)
        self.archiveCamera = self.createCamera(
            parentId = self.server.guid,
            physicalId = self.archiveCameraMac)
        self.rebuildArchive(self.server)
        
    def tearDown(self):
        MediaServerInstanceTest.tearDown(self)
        ClientMixin.tearDown(self)

    # Remove user
    def __removeUser(self, userGuid):
        self.sendAndCheckRequest(
            self.server.address, "ec2/removeUser",
            headers={'Content-Type': 'application/json'},
            data=json.dumps({'id': userGuid}))

        self.checkAbsent("ec2/getUsers",  'User', id = userGuid )
        self.checkAbsent("ec2/getResourceParams",
                           'UserResource', resourceId=userGuid)
        
    # Check camera & child object was removed
    def __checkCameraRemoved(self, cameraId):
        self.checkAbsent("ec2/getCameras", 'Camera', id = cameraId)
        self.checkAbsent("ec2/getCameraUserAttributesList", 'Camera',
                            cameraId=cameraId)
        self.checkAbsent("ec2/getResourceParams",
                           'ServerResource', resourceId=cameraId)

    # https://networkoptix.atlassian.net/browse/VMS-2246
    def testCreateAndCheck(self):
        "Create and check user"
        userData = self.createUser(
            name = "user1", email = "user1@example.com", permissions = "2432",
            cryptSha512Hash = "", digest = "", hash = "", isAdmin = False,
            isEnabled = True, isLdap = False, realm = "")
        expData = userData.get()
        expData["permissions"] = "GlobalViewArchivePermission|GlobalManageBookmarksPermission|0x80"
        
        # To make sure 
        time.sleep(2.0)
        self.checkPresent("ec2/getUsers", 'User', **expData)

        self.__removeUser(userData.id)

    # https://networkoptix.atlassian.net/browse/VMS-3052
    def testNonExistUserGroup(self):
        "Unexisting user role"
        unexistsUserRole = generateGuid()
        userData1 = self.createUser(
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
        serverData = self.createServer()

        # Create storage
        storageData = self.createStorage(parentId = serverData.id)

        self.checkPresent("ec2/getStorages", 'Storage', id=storageData.id)

        # Create cameras
        cameraData1 = self.createCamera(parentId = serverData.id)
        cameraData2 = self.createCamera(parentId = serverData.id)

        # Remove camera#1
        self.sendAndCheckRequest(
            self.server.address, "ec2/removeResource",
            data=json.dumps({'id': cameraData1.id}))

        self.__checkCameraRemoved(cameraData1.id)
                
        # Remove server
        self.sendAndCheckRequest(
            self.server.address, "ec2/removeMediaServer",
            data=json.dumps({'id': serverData.id}))

        self.checkAbsent("ec2/getMediaServers", 'Server', id = serverData.id)
        self.checkAbsent("ec2/getStorages", 'Storage', id = storageData.id)
        self.checkAbsent("ec2/getMediaServerUserAttributesList",
                         'ServerAttributes', serverId=serverData.id)
        self.checkAbsent("ec2/getResourceParams",
                         'ServerResource', resourceId=serverData.id)

        self.__checkCameraRemoved(cameraData2.id)

    # https://networkoptix.atlassian.net/browse/VMS-3068
    def testServerHeader(self):
        "Do not show specific headers for unathorized client"
        unauthorized = Client()
        response = unauthorized.httpRequest(self.server.address, "ec2/testConnection")
        self.assertEqual(401, response.status)
        self.assertFalse(response.headers.get("Server"))
        self.assertTrue(response.headers.get("WWW-Authenticate"))
        response = self.client.httpRequest(self.server.address, "ec2/testConnection")
        self.assertEqual(200, response.status)
        self.assertTrue(response.headers.get("Server"))

    # https://networkoptix.atlassian.net/browse/VMS-3069
    def testStaticVulnerability(self):
        "Directory traversal"
        dataPath = self.server.relativePath('data')
        # To make relative path valid
        os.makedirs(os.path.join(dataPath, 'web', 'static'))
        testFile = os.path.join(dataPath, 'test.file')
        with file(testFile, 'w') as f:
            print >> f, "This is just a test file"
        response = self.client.httpRequest(
            self.server.address, urllib.quote("static/../../test.file"))
        self.assertEqual(403, response.status)

    def testGetTimePeriods(self):
        'Get time periods check'
        response = self.sendAndCheckRequest(
            self.server.address,
            'ec2/recordedTimePeriods',
            cameraId = self.archiveCamera.id,
            startTime = '0',
            endTime='now')

        gotPeriods = filter(lambda x: x['guid'] == self.server.guid, response.data['reply'])
        gotPeriods = map(lambda x: x['periods'], gotPeriods)
        gotPeriods = [Rec(item) for sublist in gotPeriods for item in sublist]
        self.compare(self.expPeriods, gotPeriods)

        
