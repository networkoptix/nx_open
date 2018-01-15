from hashlib import md5

from netaddr import IPAddress

BASE_CAMERA_IP_ADDRESS = IPAddress('192.168.0.0')
BASE_SERVER_IP_ADDRESS = IPAddress('10.10.0.0')
BASE_STORAGE_IP_ADDRESS = IPAddress('10.2.0.0')
CAMERA_SERVER_GUID_PREFIX = "ca14e2a0-8e25-e200-0000"
SERVER_GUID_PREFIX = "8e25e200-0000-0000-0000"
USER_GUID_PREFIX = "58e20000-0000-0000-0000"
STORAGE_GUID_PREFIX = "81012a6e-0000-0000-0000"
LAYOUT_GUID_PREFIX = "1a404100-0000-0000-0000"
LAYOUT_ITEM_GUID_PREFIX = "1a404100-11e1-1000-0000"
CAMERA_MAC_PREFIX = "CA:14:"


def generate_camera_server_guid(id, quoted=True):
    guid = "%s-%012d" % (CAMERA_SERVER_GUID_PREFIX, id)
    return "{%s}" % guid if quoted else guid


def generate_server_guid(id, quoted=True):
    guid = "%s-%012d" % (SERVER_GUID_PREFIX, id)
    return "{%s}" % guid if quoted else guid


def generate_user_guid(id, quoted=True):
    guid = "%s-%012d" % (USER_GUID_PREFIX, id)
    return "{%s}" % guid if quoted else guid


def generate_storage_guid(id, quoted=True):
    guid = "%s-%012d" % (STORAGE_GUID_PREFIX, id)
    return "{%s}" % guid if quoted else guid


def generate_layout_guid(id, quoted=True):
    guid = "%s-%012d" % (LAYOUT_GUID_PREFIX, id)
    return "{%s}" % guid if quoted else guid


def generate_layout_item_guid(id):
    guid = "%s-%012d" % (LAYOUT_ITEM_GUID_PREFIX, id)
    return "{%s}" % guid


def generate_mac(id):
    return CAMERA_MAC_PREFIX + ":".join(map(lambda n: "%02X" % (id >> n & 0xFF), [24, 16, 8, 0]))


def generate_name(prefix, id):
    return "%s_%s" % (prefix, id)


def generate_uuid_from_string(salt):
    v = md5(salt).digest()
    return "{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}" % tuple(ord(b) for b in v)


def generate_email(name):
    return "%s@gmail.com" % name.lower()


def generate_ip_v4(id, base_address):
    assert isinstance(base_address, IPAddress)
    return str(base_address + id)


def generate_ip_v4_endpoint(id):
    return "%s:7001" % generate_ip_v4(id, BASE_SERVER_IP_ADDRESS)


def generate_password_and_digest(name):
    password = name
    d = md5("%s:NetworkOptix:%s" % (name, password)).digest()
    return (password, ''.join('%02x' % ord(i) for i in d))


def generate_password_hash(password):
    salt = 'd0d7971d'
    d = md5(salt + password).digest()
    md5_digest = ''.join('%02x' % ord(i) for i in d)
    return "md5$%s$%s" % (salt, md5_digest)


def generate_camera_data(camera_id, **kw):
    mac = kw.get('physicalId', generate_mac(camera_id))
    camera_name = kw.get('name', generate_name('ec2_test_cam', camera_id))
    default_camera_data = dict(
       audioEnabled=camera_id % 2,
       controlEnabled=camera_id % 3,
       dewarpingParams="",
       groupId='',
       groupName='',
       id=generate_uuid_from_string(mac),
       mac=mac,
       manuallyAdded=False,
       maxArchiveDays=0,
       minArchiveDays=0,
       model=camera_name,
       motionMask='',
       motionType='MT_Default',
       name=camera_name,
       parentId=generate_camera_server_guid(camera_id),
       physicalId=mac,
       preferedServerId='{00000000-0000-0000-0000-000000000000}',
       scheduleEnabled=False,
       scheduleTasks=[],
       secondaryStreamQuality='SSQualityLow',
       status='Unauthorized',
       statusFlags='CSF_NoFlags',
       typeId='{1b7181ce-0227-d3f7-9443-c86aab922d96}',
#        typeId='{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}',
       url=generate_ip_v4(camera_id, BASE_CAMERA_IP_ADDRESS),
       vendor="NetworkOptix")
    return dict(default_camera_data, **kw)


def generate_user_data(user_id, **kw):
    user_name = kw.get('name', generate_name('User', user_id))
    password, digest = generate_password_and_digest(user_name)
    default_user_data = dict(
        digest=digest,
        email=generate_email(user_name),
        hash=generate_password_hash(password),
        id=generate_user_guid(user_id),
        isAdmin=False,
        name=user_name,
        parentId='{00000000-0000-0000-0000-000000000000}',
        permissions='NoGlobalPermissions',
        userRoleId='{00000000-0000-0000-0000-000000000000}',
        typeId='{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}',
        url='')
    return dict(default_user_data, **kw)


def generate_mediaserver_data(server_id, **kw):
    server_address = kw.get('networkAddresses', generate_ip_v4_endpoint(server_id))
    server_name = kw.get('name', generate_name('Server', server_id))
    default_server_data = dict(
        apiUrl=server_address,
        url='rtsp://%s' % server_address,
        authKey=generate_uuid_from_string(server_name),
        flags='SF_HasPublicIP',
        id=generate_server_guid(server_id),
        name=server_name,
        networkAddresses=server_address,
        panicMode='PM_None',
        parentId='{00000000-0000-0000-0000-000000000000}',
        systemInfo='windows x64 win78',
        systemName=server_name,
        version='3.0.0.0'
        )
    return dict(default_server_data, **kw)


def generate_camera_user_attributes_data(camera, **kw):
    dewarpingParams = '''{"enabled":false,"fovRot":0,
    "hStretch":1,"radius":0.5,"viewMode":"1","xCenter":0.5,"yCenter":0.5}'''
    default_camera_data = dict(
        audioEnabled=False,
        backupType='CameraBackupDefault',
        cameraId=camera['id'],
        cameraName=camera['name'],
        controlEnabled=True,
        dewarpingParams=dewarpingParams,
        failoverPriority='Medium',
        licenseUsed=True,
        maxArchiveDays=-30,
        minArchiveDays=-1,
        motionMask='5,0,0,44,32:5,0,0,44,32:5,0,0,44,32:5,0,0,44,32',
        motionType='MT_SoftwareGrid',
        preferredServerId='{00000000-0000-0000-0000-000000000000}',
        scheduleEnabled=False,
        scheduleTasks=[],
        secondaryStreamQuality='SSQualityMedium',
        userDefinedGroupName=''
        )
    return dict(default_camera_data, **kw)


def generate_storage_data(storage_id, **kw):
    default_storage_data = dict(
        id=generate_storage_guid(storage_id),
        name=generate_name('Storage', storage_id),
        # By default we use samba storage to avoid accidentally local storage creating
        url='smb://%s' % generate_ip_v4(storage_id, BASE_STORAGE_IP_ADDRESS),
        storageType='smb',
        usedForWriting=False,
        isBackup=False)
    return dict(default_storage_data, **kw)


def generate_layout_item(item_id, resource_id):
    return dict(
        id=generate_layout_item_guid(item_id),
        left=0.0, top=0.0, right=0.0, bottom=0.0,
        rotation=0.0,
        resourceId=resource_id,
        resourcePath='', zoomLeft='', zoomTop='', zoomRight='',
        zoomBottom='', zoomTargetId='', displayInfo=False)


def generate_layout_data(layout_id, **kw):
    default_layout_data = dict(
        id=generate_layout_guid(layout_id),
        name=generate_name('Layout', layout_id),
        locked=False,
        items=[])
    return dict(default_layout_data, **kw)


def generate_mediaserver_user_attributes_data(server, **kw):
    return dict(
        allowAutoRedundancy=False,
        maxCameras=200,
        serverId=server['id'],
        serverName=server['name'])


def get_resource_id(resource):
    if isinstance(resource, dict):
        return resource['id']
    return resource


def generate_resource_params_data(id, resource):
    resource_id = get_resource_id(resource)
    return dict(
        name='Resource_%s_%d' % (resource_id, id),
        resourceId=resource_id,
        value='Value_%s_%d' % (resource_id, id))


def generate_resource_params_data_list(id, resource, list_size):
    return [generate_resource_params_data(id + i, resource)
            for i in range(list_size)]


def generate_remove_resource_data(resource):
    resource_id = get_resource_id(resource)
    return dict(id=resource_id)
