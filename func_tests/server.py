import os.path
import StringIO
import ConfigParser
import base64
import urllib
import hashlib
import time
import datetime
import calendar
import pytz
import tzlocal
import requests.exceptions
from server_rest_api import REST_API_USER, REST_API_PASSWORD, REST_API_TIMEOUT_SEC, ServerRestApi
from camera import Camera, SampleMediaFile


# currently expected in current directory:
MEDIASERVER_DIST_FPATH = 'networkoptix-mediaserver.deb'

MEDIASERVER_CONFIG_PATH = '/opt/networkoptix/mediaserver/etc/mediaserver.conf'
MEDIASERVER_CONFIG_PATH_INITIAL = '/opt/networkoptix/mediaserver/etc/mediaserver.conf.initial'
MEDIASERVER_FORWARDED_PORT_BASE = 7700
MEDIASERVER_LISTEN_PORT = 7001
MEDIASERVER_SERVICE_NAME = 'networkoptix-mediaserver'
MEDIASERVER_UNSETUP_LOCAL_SYSTEM_ID = '{00000000-0000-0000-0000-000000000000}'  # local system id for not set up server


class TimePeriod(object):

    def __init__(self, start, duration):
        assert isinstance(start, datetime.datetime), repr(start)
        assert isinstance(duration, datetime.timedelta), repr(duration)
        self.start = start
        self.duration = duration

    def __repr__(self):
        return 'TimePeriod(%s, %s)' % (self.start, self.duration)

    def __eq__(self, other):
        return (isinstance(other, TimePeriod)
                and other.start == self.start
                and other.duration == self.duration)


def generate_auth_key(method, user, password, nonce, realm):
    ha1 = hashlib.md5(':'.join([user.lower(), realm, password])).hexdigest()
    ha2 = hashlib.md5(':'.join([method, ''])).hexdigest()  # only method, with empty url part
    return hashlib.md5(':'.join([ha1, nonce, ha2])).hexdigest()

def change_mediaserver_config(old_config, **kw):
    config = ConfigParser.SafeConfigParser()
    config.optionxform = str  # make it case-sensitive, server treats it this way (yes, this is a bug)
    config.readfp(StringIO.StringIO(old_config))
    for name, value in kw.items():
        config.set('General', name, unicode(value))
    f = StringIO.StringIO()
    config.write(f)
    return f.getvalue()


class Server(object):

    def __init__(self, name, box, ip_address):
        self.name = name
        self.box = box
        self.url = 'http://%s:%d/' % (ip_address, MEDIASERVER_LISTEN_PORT)
        self.rest = ServerRestApi(self.url)
        self.user = REST_API_USER
        self.password = REST_API_PASSWORD
        self.settings = None

    def __repr__(self):
        return 'Server%r@%s' % (self.name, self.url)

    def start(self):
        pass

    def run_service_action(self, action):
        self.box.run_ssh_command(['service', MEDIASERVER_SERVICE_NAME, action])

    def restart_service(self):
        self.run_service_action('restart')

    def stop_service(self):
        self.run_service_action('stop')
        self.wait_until_server_is_down()

    def start_service(self):
        self.run_service_action('start')
        self.wait_until_server_is_up()

    def restart(self):
        self.rest.api.restart.get()
        self.wait_until_server_is_down()
        self.wait_until_server_is_up()

    def change_config(self, **kw):
        old_config = self.box.get_file(MEDIASERVER_CONFIG_PATH)
        new_config = change_mediaserver_config(old_config, **kw)
        self.box.put_file(MEDIASERVER_CONFIG_PATH, new_config)

    def wait_until_server_is_up(self):
        wait_time_sec = 30
        start_time = time.time()
        while time.time() < start_time + wait_time_sec:
            try:
                self.rest.api.ping.get()
                print 'Server is up now.'
                return
            except (requests.exceptions.Timeout, requests.exceptions.ConnectionError):
                print 'Server is still down...'
                time.sleep(1)
        else:
            raise RuntimeError('Server did not started in %d seconds' % wait_time_sec)

    def wait_until_server_is_down(self):
        wait_time_sec = 30
        start_time = time.time()
        while time.time() < start_time + wait_time_sec:
            try:
                self.rest.api.ping.get()
                print 'Server is still up...'
                time.sleep(1)
            except (requests.exceptions.Timeout, requests.exceptions.ConnectionError):
                print 'Server is down now.'
                return
        else:
            raise RuntimeError('Server did not started in %d seconds' % wait_time_sec)

    def setup_local_system(self, name=None):
        self.rest.api.setupLocalSystem.post(systemName=name or self.name, password=REST_API_PASSWORD)  # leave password unchanged

    def change_system_id(self, new_guid):
        old_local_system_id = self.local_system_id
        self.rest.api.configure.get(localSystemId=new_guid)
        self.load_system_settings()
        assert self.local_system_id != old_local_system_id
        assert self.local_system_id == new_guid

    def get_system_settings(self):
        response = self.rest.api.systemSettings.get()
        return response['settings']

    def load_system_settings(self):
        self.settings = self.get_system_settings()
        self.local_system_id = self.settings['localSystemId']
        self.ecs_guid = self.rest.ec2.testConnection.get()['ecsGuid']

    def is_system_set_up(self):
        return self.settings['localSystemId'] != MEDIASERVER_UNSETUP_LOCAL_SYSTEM_ID

    def get_nonce(self):
        response = self.rest.api.getNonce.get()
        return (response['realm'], response['nonce'])

    def merge_systems(self, other_server):
        realm, nonce = self.get_nonce()
        def make_key(method):
            digest = generate_auth_key(method, self.user.lower(), self.password, nonce, realm)
            return urllib.quote(base64.urlsafe_b64encode(':'.join([self.user.lower(), nonce, digest])))
        self.rest.api.mergeSystems.get(
            url=other_server.url,
            getKey=make_key('GET'),
            postKey=make_key('POST'),
            )

    def reset(self):
        self.box.run_ssh_command(['cp', MEDIASERVER_CONFIG_PATH_INITIAL, MEDIASERVER_CONFIG_PATH])
        self.change_config(removeDbOnStartup=1)
        self.restart()

    def add_camera(self, camera):
        params = camera.get_info(parent_id=self.ecs_guid)
        result = self.rest.ec2.saveCamera.post(**params)
        return result['id']

    # if there are more than one return first
    def get_storage(self):
        storage_records = [record for record in self.rest.ec2.getStorages.get() if record['parentId'] == self.ecs_guid]
        assert len(storage_records) >= 1, 'No storages for server with ecs guid %d is returned by %s' % (self.ecs_guid, self.url)
        return Storage(self.box, storage_records[0]['url'])

    def rebuild_archive(self):
        self.rest.api.rebuildArchive.get(mainPool=1, action='start')
        for i in range(30):
            response = self.rest.api.rebuildArchive.get(mainPool=1)
            if response['state'] == 'RebuildState_None':
                return
            time.sleep(0.3)
        assert False, 'Timed out waiting for archive to rebuild'

    def get_recorded_time_periods(self, camera_id):
        periods = [TimePeriod(datetime.datetime.utcfromtimestamp(int(d['startTimeMs'])/1000.).replace(tzinfo=pytz.utc),
                    datetime.timedelta(seconds=int(d['durationMs'])/1000.))
                    for d in self.rest.ec2.recordedTimePeriods.get(cameraId=camera_id, flat=True)]
        print 'Server %r returned recorded periods:' % self.name
        print '\t' + '\n\t'.join(map(str, periods))
        return periods


class Storage(object):

    def __init__(self, box, dir):
        self.box = box
        self.dir = dir

    def cleanup(self):
        self.box.run_ssh_command(['rm', '-rf', os.path.join(self.dir, 'low_quality'), os.path.join(self.dir, 'hi_quality')])

    def save_media_sample(self, camera, start_time, sample):
        assert isinstance(camera, Camera), repr(camera)
        assert isinstance(start_time, datetime.datetime) and start_time.tzinfo, repr(start_time)
        assert start_time.tzinfo  # naive datetime are forbidden, use pytz.utc or tzlocal.get_localtimezone() for tz
        assert isinstance(sample, SampleMediaFile), repr(sample)
        camera_mac_addr = camera.mac_addr
        contents = sample.get_contents()
        lowq_fpath = self._construct_fpath(camera_mac_addr, 'low_quality', start_time, sample.duration)
        hiq_fpath  = self._construct_fpath(camera_mac_addr, 'hi_quality',  start_time, sample.duration)
        self.box.put_file(lowq_fpath, contents)
        self.box.put_file(hiq_fpath,  contents)

    # server stores media data in this format, using local time for directory parts:
    # <data dir>/<{hi_quality,low_quality}>/<camera-mac>/<year>/<month>/<day>/<hour>/<start,unix timestamp ms>_<duration,ms>.mkv
    # for example:
    # server/var/data/data/low_quality/urn_uuid_b0e78864-c021-11d3-a482-f12907312681/2017/01/27/12/1485511093576_21332.mkv
    def _construct_fpath(self, camera_mac_addr, quality_part, start_time, duration):
        local_dt = start_time.astimezone(self.box.timezone)  # box local
        unixtime_utc_ms = calendar.timegm(start_time.utctimetuple())*1000 + start_time.microsecond/1000
        duration_ms = int(duration.total_seconds() * 1000)
        return os.path.join(
            self.dir, quality_part, camera_mac_addr,
            '%02d' % local_dt.year, '%02d' % local_dt.month, '%02d' % local_dt.day, '%02d' % local_dt.hour,
            '%s_%s.mkv' % (unixtime_utc_ms, duration_ms))
