import hashlib
import hachoir_core.config
# overwise hachoir will replace sys.stdout/err with UnicodeStdout, incompatible with pytest terminal module:
hachoir_core.config.unicode_stdout = False
import hachoir_parser
import hachoir_metadata
        

CAMERA_MAC_ADDR = '11:22:33:44:55:66'


# obsolete, not used anymore; strange server requirements for camera id
def generate_camera_id_from_mac_addr(mac_addr):
    v = hashlib.md5(mac_addr).digest()
    return '{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}' % tuple(ord(b) for b in v)

def make_camera_info(parent_id, name, mac_addr):
    return dict(
        audioEnabled=False,
        controlEnabled=False,
        dewarpingParams='',
        groupId='',
        groupName='',
        mac=mac_addr,
        manuallyAdded=False,
        maxArchiveDays=0,
        minArchiveDays=0,
        model=name,
        motionMask='',
        motionType='MT_Default',
        name=name,
        parentId=parent_id,
        physicalId=mac_addr,
        preferedServerId='{00000000-0000-0000-0000-000000000000}',
        scheduleEnabled=False,
        scheduleTasks=[],
        secondaryStreamQuality='SSQualityLow',
        status='Unauthorized',
        statusFlags='CSF_NoFlags',
        typeId='{7d2af20d-04f2-149f-ef37-ad585281e3b7}',
        url='127.0.0.100',
        vendor='python-funtest',
        )


class Camera(object):

    def __init__(self, name='funtest-camera', mac_addr=CAMERA_MAC_ADDR):
        self.name = name
        self.mac_addr = mac_addr

    def __repr__(self):
        return 'Camera(%s)' % self.mac_addr

    def get_info(self, parent_id):
        return make_camera_info(parent_id, self.name, self.mac_addr)


class SampleMediaFile(object):

    def __init__(self, fpath):
        metadata = self._read_metadata(fpath)
        self.fpath = fpath
        self.duration = metadata.get('duration')  # datetime.timedelta
        video_group = metadata['video[1]']
        self.width = video_group.get('width')
        self.height = video_group.get('height')

    def __repr__(self):
        return 'SampleMediaPath(%r, duration=%s)' % (self.fpath, self.duration)

    def _read_metadata(self, fpath):
        parser = hachoir_parser.createParser(unicode(fpath))
        return hachoir_metadata.extractMetadata(parser)

    def get_contents(self):
        with open(self.fpath, 'rb') as f:
            return f.read()
