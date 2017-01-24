# $Id$
# Artem V. Nikitin
# Data generators

import json, random, string, uuid
from hashlib import md5
from Rec import Rec

class _unique(object):
    "Contains some global numbers."
    SessionNumber = str(random.randint(1,1000000))
    CameraSeedNumber = 0
    UserSeedNumber = 0
    MediaServerSeedNumber = 0
    LayoutSeedNumber = 0
    UserRoleSeedNumber = 0

def generateGuid(quoted=True):
    guid = str(uuid.uuid4())
    return quoted and "{%s}" % guid or guid

class BasicGenerator(object):
    chars = string.ascii_uppercase + string.digits
    _removeTemplate = '{ "id":"%s" }'  # used in ancestors

    @staticmethod
    def generateMac():
        "Generates MAC address for an object"
        return ':'.join("%02X" % random.randint(0,255) for _ in xrange(6))

    @staticmethod
    def generateBool():
        return "true" if random.randint(0,1) == 0 else "false"

    @staticmethod
    def generateBoolVal():
        return True if random.randint(0,1) == 0 else False

    @staticmethod
    def generateRandomString(length):
        return ''.join(random.choice(BasicGenerator.chars) for _ in xrange(length))

    @staticmethod
    def generateUUIdFromMd5(salt):
        v = md5(salt).digest()
        return "{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}" % tuple(ord(b) for b in v)

    def generateRandomId(self):
        return self.generateUUIdFromMd5(self.generateRandomString(random.randint(6,12)))

    def generateIpV4(self):
        return '.'.join(str(random.randint(0,255)) for _ in xrange(4))

    def generateIpV4Endpoint(self):
        return "%s:%d" % (self.generateIpV4(),random.randint(0,65535))

    def generateEmail(self):
        return "%s@gmail.com" % (self.generateRandomString(random.randint(6,20)),)

    def generateEnum(self,*args):
        idx = random.randint(0,len(args) - 1)
        return args[idx]

    def generateUsernamePasswordAndDigest(self, namegen):
        un = namegen()
        pwd = self.generateRandomString(random.randint(8,20))
        d = md5("%s:NetworkOptix:%s" % (un,pwd)).digest()

        return (un,pwd,''.join('%02x' % ord(i) for i in d))

    def generateDigest(self, uname, pwd):
        d = md5(':'.join((uname, 'NetworkOptix', pwd))).digest()
        return ''.join("%02x" % ord(i) for i in d)

    def generatePasswordHash(self, pwd):
        salt = "%x" % (random.randint(0,4294967295))
        d = md5(salt+pwd).digest()
        md5_digest = ''.join('%02x' % ord(i) for i in d)
        return "md5$%s$%s" % (salt,md5_digest)

    def generateCameraName(self):
        ret = "ec2_test_cam_%s_%s" % (_unique.SessionNumber, _unique.CameraSeedNumber)
        _unique.CameraSeedNumber += 1
        return ret

    def generateUserName(self):
        ret = "ec2_test_user_%s_%s" % (_unique.SessionNumber, _unique.UserSeedNumber)
        _unique.UserSeedNumber += 1
        return ret

    def generateMediaServerName(self):
        ret = "ec2_test_media_server_%s_%s" % (_unique.SessionNumber, _unique.MediaServerSeedNumber)
        _unique.MediaServerSeedNumber += 1
        return ret

    def generateLayoutName(self):
        try:
            return "ec2_test_layout_%s_%s" % (_unique.SessionNumber, _unique.LayoutSeedNumber)
        finally:
            _unique.LayoutSeedNumber += 1

    def generateUserRoleName(self):
        try:
            return "ec2_test_user_role_%s_%s" % (_unique.SessionNumber, _unique.UserRoleSeedNumber)
        finally:
            _unique.UserRoleSeedNumber += 1

def generateServerData(**kw):
    gen = BasicGenerator()
    srvAddr = kw.get('networkAddresses', gen.generateIpV4Endpoint())
    default_server_data = {
        'apiUrl': srvAddr, 'url' : "rtsp://%s" % srvAddr,
        'authKey': gen.generateRandomId(), 'flags' : "SF_HasPublicIP",
        'id': generateGuid(), 'name': gen.generateMediaServerName(),
        'networkAddresses': srvAddr, 'panicMode' : "PM_None",
        'parentId' : "{00000000-0000-0000-0000-000000000000}",
        'systemInfo': "windows x64 win78",
        'systemName': gen.generateRandomString(random.randint(5,20)),
        'version': "3.0.0.0" }
    return Rec(init=default_server_data, **kw)


def generateStorageData(**kw):
    gen = BasicGenerator()
    default_storage_data = {
        'id': generateGuid(),
        'parentId': generateGuid(),
        'name': gen.generateRandomString(10),
        'url': "smb://%s" % gen.generateIpV4Endpoint(),
        'spaceLimit': random.randint(50000,1000000),
        'usedForWriting': False, 'storageType': "smb",
        'addParams': [], 'isBackup': False }
    return Rec(init=default_storage_data, **kw)

def generateCameraData(**kw):
    gen = BasicGenerator()
    mac = kw.get('physicalId', gen.generateMac())
    name = kw.get('name', gen.generateCameraName())
    default_camera_data = {
        "audioEnabled": gen.generateBoolVal(),
        "controlEnabled": gen.generateBoolVal(),
        "dewarpingParams": "",  "groupId": "",
        "groupName": "",
        "id": gen.generateUUIdFromMd5(mac),
        "mac": mac,
        "manuallyAdded": False,  "maxArchiveDays": 0,
        "minArchiveDays": 0, "model": name,
        "motionMask": "", "motionType": "MT_Default",
        "name": name, "parentId": generateGuid(),
        "physicalId":  mac,
        "preferedServerId": "{00000000-0000-0000-0000-000000000000}",
        "scheduleEnabled": False, "scheduleTasks": [ ],
        "secondaryStreamQuality": "SSQualityLow",
        "status": "Unauthorized", "statusFlags": "CSF_NoFlags",
        "typeId": "{7d2af20d-04f2-149f-ef37-ad585281e3b7}",
        "url": gen.generateIpV4(), "vendor": gen.generateRandomString(4) }
    return Rec(init=default_camera_data, **kw)

def generateServerUserAttributesData(**kw):
    gen = BasicGenerator()
    default_server_user_attr_data = {
        "allowAutoRedundancy": gen.generateBoolVal(),
        "maxCameras": random.randint(0,200), "serverId": generateGuid(),
        "serverName": gen.generateRandomString(random.randint(5,20)) }
    return Rec(init=default_server_user_attr_data, **kw)

def generateCameraUserAttributesData(**kw):
    gen = BasicGenerator()
    dewarpingParams = '''{ \\"enabled\\":%s, \\"fovRot\\":%s,
    \\"hStretch\\":%s, \\"radius\\":%s, \\"viewMode\\":\\"VerticalDown\\",
    \\"xCenter\\":%s,\\"yCenter\\":%s}''' % (
        gen.generateBoolVal(),
        str(random.random()),
        str(random.random()),
        str(random.random()),
        str(random.random()),
        str(random.random()))
    default_camera_data =  {
        "audioEnabled": gen.generateBoolVal(),
        "cameraId": generateGuid(),
        "cameraName": gen.generateCameraName(),
        "controlEnabled": gen.generateBoolVal(),
        "dewarpingParams": dewarpingParams,
        "maxArchiveDays": -30,  "minArchiveDays": -1,
        "motionMask": "5,0,0,44,32:5,0,0,44,32:5,0,0,44,32:5,0,0,44,32",
        "motionType": "MT_SoftwareGrid",  "parentId": generateGuid(),
        "scheduleEnabled": False, "scheduleTasks": [ ],
        "secondaryStreamQuality": "SSQualityMedium" }
    return Rec(init=default_camera_data, **kw)

def generateResourceData(**kw):
    gen = BasicGenerator()
    default_resource_data = {
        "name": gen.generateRandomString(random.randint(4,12)),
        "resourceId": generateGuid(),
        "value": gen.generateRandomString(random.randint(24,512)) }
    return Rec(init=default_resource_data, **kw)


def generateUserData(**kw):
    gen = BasicGenerator()
    def getName():
        name = kw.get('name')
        if name: return name
        return self.generateUserName()
    un,pwd,digest = gen.generateUsernamePasswordAndDigest(getName)
    default_user_data = {
        "digest":  digest,
        "email": gen.generateEmail(),
        "hash": gen.generatePasswordHash(pwd),
        "id": generateGuid(),
        "isAdmin": False,
        "name": un,
        "parentId": "{00000000-0000-0000-0000-000000000000}",
        "permissions": "NoGlobalPermissions",
        "userRoleId": "{00000000-0000-0000-0000-000000000000}",
        "typeId": "{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}",
        "url": "" }
    return Rec(init=default_user_data, **kw)


class GeneratorMixin:

    # Create user with resource
    def createUser(self, **kw):
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

        self.checkPresent("ec2/getUsers",
                            'User', id=userData.id)

        self.checkPresent("ec2/getResourceParams",
                           'UserResource', resourceId=userData.id)
        return userData

    # Create camera with attributes & resource
    def createCamera(self, **kw):
        cameraData = generateCameraData(**kw)
        
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

        self.checkPresent("ec2/getCameras", 'Camera', id=cameraData.id)
        self.checkPresent("ec2/getCameraUserAttributesList", 'CameraAttributes',
                            cameraId=cameraData.id)
        self.checkPresent("ec2/getResourceParams",
                            'CameraResource', resourceId=cameraData.id)
        
        return cameraData

    # Create storage
    def createStorage(self, **kw):
        storageData = generateStorageData(**kw)

        self.sendAndCheckRequest(
            self.server.address, "ec2/saveStorage",
            data=json.dumps(storageData.get()))

        self.checkPresent("ec2/getStorages", 'Storage', id=storageData.id)

        return storageData

    # Create server with attributes & resource
    def createServer(self, **kw):
        serverData = generateServerData(**kw)

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

        self.checkPresent("ec2/getMediaServers", 'Server', id=serverData.id)
        self.checkPresent("ec2/getMediaServerUserAttributesList",
                          'ServerAttributes', serverId=serverData.id)
        self.checkPresent("ec2/getResourceParams",
                          'ServerResource', resourceId=serverData.id)
        return serverData

    # Check entity (server, camera, user, etc) present
    def checkPresent(self, __method, __name, **kw):
        response = self.sendAndCheckRequest(
            self.server.address, __method)
        
        self.assertHasItem(kw, response.data, __name)

    # Check entity (server, camera, user, etc) absent
    def checkAbsent(self, __method, __name, **kw):
        response = self.sendAndCheckRequest(
            self.server.address, __method)
        
        self.assertHasNotItem(kw, response.data, __name)


