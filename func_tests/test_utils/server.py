'''mediaserver presentation class

Allows working with servers from functional tests - start/stop, setup, configure, access rest api, storage, etc.
'''

import os.path
import logging
import base64
import uuid
import urllib
import hashlib
import time
import datetime
import calendar
import pytz
import tzlocal
import requests.exceptions
import pytest
from .utils import is_list_inst, datetime_utc_to_timestamp
from .server_rest_api import REST_API_USER, REST_API_PASSWORD, REST_API_TIMEOUT_SEC, HttpError, ServerRestApi
from .vagrant_box_config import MEDIASERVER_LISTEN_PORT, BoxConfigFactory, BoxConfig
from .cloud_host import CloudHost
from .camera import make_schedule_task, Camera, SampleMediaFile
from .media_stream import open_media_stream
from .host import Host
from .cloud_host import CloudHost


DEFAULT_HTTP_SCHEMA = 'http'

MEDIASERVER_STORAGE_PATH = 'var/data'

MEDIASERVER_UNSETUP_LOCAL_SYSTEM_ID = '{00000000-0000-0000-0000-000000000000}'  # local system id for not set up server

MEDIASERVER_CREDENTIALS_TIMEOUT_SEC = 60 * 5
MEDIASERVER_MERGE_TIMEOUT_SEC = MEDIASERVER_CREDENTIALS_TIMEOUT_SEC  # timeout for local system ids become the same
MEDIASERVER_MERGE_REQUEST_TIMEOUT_SEC = 90  # timeout for mergeSystems REST api request
MEDIASERVER_START_TIMEOUT_SEC = 60  # timeout when waiting for server become online (pingable)

DEFAULT_SERVER_LOG_LEVEL = 'DEBUG2'


log = logging.getLogger(__name__)


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


class ServerConfig(object):

    def __init__(self, name, start=True, setup=True, leave_initial_cloud_host=False,
                 box=None, config_file_params=None, setup_settings=None, setup_cloud_host=None,
                 http_schema=None, rest_api_timeout_sec=None):
        assert name, repr(name)
        assert type(setup) is bool, repr(setup)
        assert config_file_params is None or isinstance(config_file_params, dict), repr(config_file_params)
        assert setup_settings is None or isinstance(setup_settings, dict), repr(setup_settings)
        assert setup_cloud_host is None or isinstance(setup_cloud_host, CloudHost), repr(setup_cloud_host)
        assert http_schema in [None, 'http', 'https'], repr(http_schema)
        self.name = name
        self.start = start
        self.setup = setup  # setup as local system if setup_cloud_host is None, to cloud if it is set
        # By default, by Server's 'init' method  it's hardcoded cloud host will be patched/restored to the one
        # deduced from --cloud-group option. With leave_initial_cloud_host=True, this step will be skipped.
        # With leave_initial_cloud_host=True box will also be always recreated before this test to ensure
        # server binaries has original cloud host encoded by compilation step.
        self.leave_initial_cloud_host = leave_initial_cloud_host  # bool
        self.box = box  # VagrantBox or None
        self.config_file_params = config_file_params  # dict or None
        self.setup_settings = setup_settings or {}  # dict
        self.setup_cloud_host = setup_cloud_host  # CloudHost or None
        self.http_schema = http_schema or DEFAULT_HTTP_SCHEMA  # 'http' or 'https'
        self.rest_api_timeout_sec = rest_api_timeout_sec

    def __repr__(self):
        return 'ServerConfig(%r @ %s)' % (self.name, self.box)


class Server(object):

    _st_started = object()
    _st_stopped = object()
    _st_starting = object()

    def __init__(self, name, host, installation, server_ctl, rest_api_url,
                 rest_api_timeout_sec=None, internal_ip_port=None, timezone=None):
        assert name, repr(name)
        assert isinstance(host, Host), repr(host)
        self.title = name.upper()
        self.name = '%s-%s' % (name, str(uuid.uuid4())[-12:])
        self.host = host
        self._installation = installation
        self._server_ctl = server_ctl
        self.rest_api_url = rest_api_url
        self.user = REST_API_USER
        self.password = REST_API_PASSWORD
        self.rest_api = ServerRestApi(self.title, self.rest_api_url, self.user, self.password, rest_api_timeout_sec)
        self.settings = None
        self.local_system_id = None
        self.ecs_guid = None
        self.internal_ip_port = internal_ip_port or MEDIASERVER_LISTEN_PORT
        self.internal_ip_address = None
        self.timezone = timezone
        self._state = None  # self._st_*

    def __repr__(self):
        return 'Server(%s)' % self

    def __str__(self):
        return '%r@%s' % (self.name, self.rest_api_url)

    @property
    def internal_url(self):
        return 'http://%s:%d/' % (self.internal_ip_address, self.internal_ip_port)

    @property
    def dir(self):
        return self._installation.dir

    def init(self, must_start, reset, log_level=DEFAULT_SERVER_LOG_LEVEL, patch_set_cloud_host=None, config_file_params=None):
        if self._state is None:
            was_started = self._server_ctl.get_state()
            self._state = self._st_starting if was_started else self._st_stopped
        else:
            was_started = self._state in [self._st_starting, self._st_started]
        log.info('Service for %s %s started', self, was_started and 'WAS' or 'was NOT')
        self._installation.cleanup_core_files()
        if reset:
            if was_started:
                self.stop_service()
            self._installation.cleanup_var_dir()
            if patch_set_cloud_host:
                self.patch_binary_set_cloud_host(patch_set_cloud_host)  # may be changed by previous tests...
            self.reset_config(logLevel=log_level, tranLogLevel=log_level, **(config_file_params or {}))
            if must_start:
                self.start_service()
        else:
            log.warning('Server %s will NOT be reset', self)
            self.ensure_service_status(must_start)
        if self.is_started():
            assert not reset or not self.is_system_set_up(), 'Failed to properly reinit server - it reported to be already set up'

    def list_core_files(self):
        return self._installation.list_core_files()

    def is_started(self):
        assert self._state is not None, 'server status is still unknown'
        return self._state == self._st_started

    def is_stopped(self):
        assert self._state is not None, 'server status is still unknown'
        return self._state == self._st_stopped

    def start_service(self):
        self.set_service_status(is_started=True)

    def stop_service(self):
        self.set_service_status(is_started=False)

    def restart_service(self):
        self.stop_service()
        self.start_service()

    def _bool2final_state(self, is_started):
        if is_started:
            return self._st_started
        else:
            return self._st_stopped

    def set_service_status(self, is_started):
        assert self._state != self._bool2final_state(is_started), (
            'Service for %s is already %s' % (self, is_started and 'started' or 'stopped'))
        if not (is_started and self._state == self._st_starting):
            self._server_ctl.set_state(is_started)
        self._state = self._st_starting if is_started else self._st_stopped
        if is_started:
            self.wait_for_server_become_online()
            self._state = self._st_started
            if is_started:
                self.load_system_settings()

    def ensure_service_status(self, is_started):
        if self._state != self._bool2final_state(is_started):
            self.set_service_status(is_started)

    def wait_for_server_become_online(self, check_interval_sec=0.5):
        wait_time_sec = MEDIASERVER_START_TIMEOUT_SEC
        start_time = time.time()
        while time.time() < start_time + wait_time_sec:
            response = self.is_server_online()
            if response:
                log.info('Server is online now.')
                return response
            else:
                log.debug('Server is still offline...')
                time.sleep(check_interval_sec)
        else:
            raise RuntimeError('Server %s has not went online in %d seconds' % (self, wait_time_sec))

    def is_server_online(self):
        try:
            return self.rest_api.api.ping.GET(timeout_sec=10)
        except (requests.exceptions.Timeout, requests.exceptions.ConnectionError):
            return False

    def get_log_file(self):
        return self._installation.get_log_file()

    def load_system_settings(self):
        log.debug('%s: Loading settings...', self)
        self.settings = self.get_system_settings()
        self.local_system_id = self.settings['localSystemId']
        self.cloud_system_id = self.settings['cloudSystemID']
        iflist = self.rest_api.api.iflist.GET()
        self.internal_ip_address = iflist[-1]['ipAddr']
        self.ecs_guid = self.rest_api.ec2.testConnection.GET()['ecsGuid']

    def _safe_api_call(self, fn, *args, **kw):
        try:
            return fn(*args, **kw)
        except (requests.exceptions.Timeout, requests.exceptions.ConnectionError):
            return None

    def get_system_settings(self):
        response = self.rest_api.api.systemSettings.GET()
        return response['settings']

    def is_system_set_up(self):
        return self.settings['localSystemId'] != MEDIASERVER_UNSETUP_LOCAL_SYSTEM_ID

    def get_setup_type(self):
        if not self.is_system_set_up():
            return None  # not set up
        if self.cloud_system_id:
            return 'cloud'
        else:
            return 'local'

    def reset(self):
        was_started = self._server_ctl.is_running()
        if was_started:
            self.stop_service()
        self._installation.cleanup_var_dir()
        if was_started:
            self.start_service()

    def restart(self, timeout=30):
        t = time.time()
        uptime = self.get_uptime()
        self.rest_api.api.restart.GET()
        while time.time() - t < timeout:
            new_uptime = self._safe_api_call(self.get_uptime)
            if new_uptime and new_uptime < uptime:
                self.load_system_settings()
                return
            log.debug('Server did not restart yet, waiting...')
            time.sleep(0.5)
        assert False, 'Server %r did not restart in %s seconds' % (self, timeout)

    def make_core_dump(self):
        self._server_ctl.make_core_dump()

    def get_uptime(self):
        response = self.rest_api.api.statistics.GET()
        return datetime.timedelta(seconds=int(response['uptimeMs'])/1000.)

    def reset_config(self, **kw):
        self._installation.reset_config(**kw)

    def change_config(self, **kw):
        self._installation.change_config(**kw)

    def patch_binary_set_cloud_host(self, new_host):
        assert self.is_stopped(), 'Server %s must be stopped first for patching its binaries' % self
        self._installation.patch_binary_set_cloud_host(new_host)
        self.set_user_password(REST_API_USER, REST_API_PASSWORD)  # Must be reset to default ones

    def set_system_settings(self, **kw):
        self.rest_api.api.systemSettings.GET(**kw)
        self.load_system_settings()

    def setup_local_system(self, **kw):
        self.rest_api.api.setupLocalSystem.POST(systemName=self.name, password=REST_API_PASSWORD, **kw)  # leave password unchanged
        self.load_system_settings()

    def setup_cloud_system(self, cloud_host, **kw):
        assert isinstance(cloud_host, CloudHost), repr(cloud_host)
        bind_info = cloud_host.bind_system(self.name)
        setup_response = self.rest_api.api.setupCloudSystem.POST(
            systemName=self.name,
            cloudAuthKey=bind_info.auth_key,
            cloudSystemID=bind_info.system_id,
            cloudAccountName=cloud_host.user,
            timeout_sec=60*5,
            **kw)
        settings = setup_response['settings']
        self.set_user_password(cloud_host.user, cloud_host.password)
        self.load_system_settings()
        assert self.settings['systemName'] == self.name
        return settings

    def detach_from_cloud(self, password):
        self.rest_api.api.detachFromCloud.POST(password=password)
        self.set_user_password(REST_API_USER, password)

    def merge(self, other_server_list):
        assert is_list_inst(other_server_list, Server), repr(other_server_list)
        for server in other_server_list:
            self.merge_systems(server)

    def merge_systems(self, other_server, take_remote_settings=False):
        log.info('Merging servers: %s with local_system_id=%r and %s with local_system_id=%r',
                 self, self.local_system_id, other_server, other_server.local_system_id)
        assert self.is_started() and other_server.is_started()
        assert self.local_system_id is not None and other_server is not None  # expected to be loaded already
        assert self.local_system_id != other_server.local_system_id  # Must not be merged yet

        realm, nonce = other_server.get_nonce()
        def make_key(method):
            digest = generate_auth_key(method, other_server.user.lower(), other_server.password, nonce, realm)
            return urllib.quote(base64.urlsafe_b64encode(':'.join([other_server.user.lower(), nonce, digest])))
        self.rest_api.api.mergeSystems.GET(
            url=other_server.internal_url,
            getKey=make_key('GET'),
            postKey=make_key('POST'),
            takeRemoteSettings=take_remote_settings,
            timeout_sec=MEDIASERVER_MERGE_REQUEST_TIMEOUT_SEC,
            )
        if take_remote_settings:
            self.set_user_password(other_server.user, other_server.password)
        else:
            other_server.set_user_password(self.user, self.password)
        self._wait_for_servers_to_merge(other_server)
        time.sleep(5)  # servers still need some time to settle down; hope this time will be enough

    def _wait_for_servers_to_merge(self, other_server):
        t = time.time()
        while time.time() - t < MEDIASERVER_MERGE_TIMEOUT_SEC:
            self.load_system_settings()
            other_server.load_system_settings()
            if self.local_system_id == other_server.local_system_id:
                log.info('Servers are merged now, have common local_system_id=%r', self.local_system_id)
                return
            log.debug('Servers are not merged yet, waiting...')
            time.sleep(0.5)
        pytest.fail('Timed out in %s seconds waiting for servers %s and %s to be merged'
                    % (MEDIASERVER_MERGE_TIMEOUT_SEC, self, other_server))

    def set_user_password(self, user, password):
        log.debug('%s got user/password: %r/%r', self, user, password)
        self.user = user
        self.password = password
        self.rest_api.set_credentials(self.user, self.password)
        if self.is_started():
            self._wait_for_credentials_accepted()

    # wait until existing server credentials become valid
    def _wait_for_credentials_accepted(self):
        t = time.time()
        while time.time() - t < MEDIASERVER_CREDENTIALS_TIMEOUT_SEC:
            try:
                self.rest_api.ec2.testConnection.GET()
                log.info('Server accepted new credentials in %s seconds', time.time() - t)
                return
            except HttpError as x:
                if x.status_code != 401:
                    raise
            time.sleep(1)
        pytest.fail('Timed out in %s seconds waiting for server %s to accept credentials: user=%r, password=%r'
                    % (MEDIASERVER_CREDENTIALS_TIMEOUT_SEC, self, self.user, self.password))

    def get_nonce(self):
        response = self.rest_api.api.getNonce.GET()
        return (response['realm'], response['nonce'])

    def change_system_id(self, new_guid):
        old_local_system_id = self.local_system_id
        self.rest_api.api.configure.GET(localSystemId=new_guid)
        self.load_system_settings()
        assert self.local_system_id != old_local_system_id
        assert self.local_system_id == new_guid

    def add_camera(self, camera):
        assert not camera.id, 'Already added to a server with id %r' % camera.id
        params = camera.get_info(parent_id=self.ecs_guid)
        result = self.rest_api.ec2.saveCamera.POST(**params)
        camera.id = result['id']
        return camera.id

    def set_camera_recording(self, camera, recording):
        assert camera, 'Camera %r is not yet registered on server' % camera
        schedule_tasks=[make_schedule_task(day_of_week + 1) for day_of_week in range(7)]
        self.rest_api.ec2.saveCameraUserAttributes.POST(
            cameraId=camera.id, scheduleEnabled=recording, scheduleTasks=schedule_tasks)

    def start_recording_camera(self, camera):
        self.set_camera_recording(camera, recording=True)

    def stop_recording_camera(self, camera):
        self.set_camera_recording(camera, recording=False)

    # if there are more than one return first
    @property
    def storage(self):
        ## # following code requires server is started, which is not always the case;
        ## #  so was commented-out and replaced with hardcoded one
        ## storage_records = [record for record in self.rest_api.ec2.getStorages.GET() if record['parentId'] == self.ecs_guid]
        ## assert len(storage_records) >= 1, 'No storages for server with ecs guid %s is returned by %s' % (self.ecs_guid, self.rest_api_url)
        ## storage_path = storage_records[0]['url']
        storage_path = os.path.join(self._installation.dir, MEDIASERVER_STORAGE_PATH)
        return Storage(self.host, storage_path, self.timezone)

    def rebuild_archive(self):
        self.rest_api.api.rebuildArchive.GET(mainPool=1, action='start')
        for i in range(30):
            response = self.rest_api.api.rebuildArchive.GET(mainPool=1)
            if response['state'] == 'RebuildState_None':
                return
            time.sleep(0.3)
        assert False, 'Timed out waiting for archive to rebuild'

    def get_recorded_time_periods(self, camera):
        assert camera.id, 'Camera %r is not yet registered on server' % camera.name
        periods = [TimePeriod(datetime.datetime.utcfromtimestamp(int(d['startTimeMs'])/1000.).replace(tzinfo=pytz.utc),
                    datetime.timedelta(seconds=int(d['durationMs'])/1000.))
                    for d in self.rest_api.ec2.recordedTimePeriods.GET(cameraId=camera.id, flat=True)]
        log.info('Server %r returned %d recorded periods:', self.name, len(periods))
        for period in periods:
            log.info('\t%s', period)
        return periods

    def get_media_stream(self, stream_type, camera):
        assert stream_type in ['rtsp', 'webm', 'hls', 'direct-hls'], repr(stream_type)
        assert isinstance(camera, Camera), repr(camera)
        return open_media_stream(self.rest_api_url, self.user, self.password, stream_type, camera.mac_addr)


class Storage(object):

    def __init__(self, host, dir, timezone=None):
        self.host = host
        self.dir = dir
        self.timezone = timezone or host.get_timezone()

    def cleanup(self):
        self.host.run_command(['rm', '-rf',
                               os.path.join(self.dir, 'low_quality'),
                               os.path.join(self.dir, 'hi_quality')])

    def save_media_sample(self, camera, start_time, sample):
        assert isinstance(camera, Camera), repr(camera)
        assert isinstance(start_time, datetime.datetime) and start_time.tzinfo, repr(start_time)
        assert start_time.tzinfo  # naive datetime are forbidden, use pytz.utc or tzlocal.get_localtimezone() for tz
        assert isinstance(sample, SampleMediaFile), repr(sample)
        camera_mac_addr = camera.mac_addr
        contents = sample.get_contents()
        lowq_fpath = self._construct_fpath(camera_mac_addr, 'low_quality', start_time, sample.duration)
        hiq_fpath  = self._construct_fpath(camera_mac_addr, 'hi_quality',  start_time, sample.duration)
        self.host.write_file(lowq_fpath, contents)
        self.host.write_file(hiq_fpath,  contents)

    # server stores media data in this format, using local time for directory parts:
    # <data dir>/<{hi_quality,low_quality}>/<camera-mac>/<year>/<month>/<day>/<hour>/<start,unix timestamp ms>_<duration,ms>.mkv
    # for example:
    # server/var/data/data/low_quality/urn_uuid_b0e78864-c021-11d3-a482-f12907312681/2017/01/27/12/1485511093576_21332.mkv
    def _construct_fpath(self, camera_mac_addr, quality_part, start_time, duration):
        local_dt = start_time.astimezone(self.timezone)  # box local
        unixtime_utc_ms = int(datetime_utc_to_timestamp(start_time) * 1000)
        duration_ms = int(duration.total_seconds() * 1000)
        return os.path.join(
            self.dir, quality_part, camera_mac_addr,
            '%02d' % local_dt.year, '%02d' % local_dt.month, '%02d' % local_dt.day, '%02d' % local_dt.hour,
            '%s_%s.mkv' % (unixtime_utc_ms, duration_ms))
