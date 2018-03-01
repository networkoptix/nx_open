"""Work with mediaserver as single entity: start/stop, setup, configure, access HTTP api, storage, etc..."""

import base64
import datetime
import hashlib
import logging
import tempfile
import time
import urllib
import uuid
from pprint import pformat

import pytest
import pytz
import requests.exceptions
from netaddr.ip import IPAddress, IPNetwork
from pathlib2 import Path

from test_utils.api_shortcuts import get_local_system_id, get_server_id, get_settings
from test_utils.utils import Wait, wait_until
from .camera import Camera, SampleMediaFile, make_schedule_task
from .cloud_host import CloudAccount
from .media_stream import open_media_stream
from .os_access import LocalAccess
from .rest_api import HttpError, REST_API_PASSWORD, REST_API_USER
from .utils import RunningTime, datetime_utc_now, datetime_utc_to_timestamp, is_list_inst
from .vagrant_vm_config import MEDIASERVER_LISTEN_PORT

DEFAULT_HTTP_SCHEMA = 'http'

MEDIASERVER_STORAGE_PATH = 'var/data'

MEDIASERVER_CREDENTIALS_TIMEOUT = datetime.timedelta(minutes=5)
MEDIASERVER_MERGE_TIMEOUT = MEDIASERVER_CREDENTIALS_TIMEOUT  # timeout for local system ids become the same
MEDIASERVER_MERGE_REQUEST_TIMEOUT = datetime.timedelta(seconds=90)  # timeout for mergeSystems REST api request
MEDIASERVER_START_TIMEOUT = datetime.timedelta(minutes=5)  # timeout when waiting for server become online (pingable)

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

    def __init__(self, name, setup=True, leave_initial_cloud_host=False,
                 vm=None, config_file_params=None, setup_settings=None, setup_cloud_account=None,
                 http_schema=None, rest_api_timeout=None):
        assert name, repr(name)
        assert type(setup) is bool, repr(setup)
        assert config_file_params is None or isinstance(config_file_params, dict), repr(config_file_params)
        assert setup_settings is None or isinstance(setup_settings, dict), repr(setup_settings)
        assert setup_cloud_account is None or isinstance(setup_cloud_account, CloudAccount), repr(setup_cloud_account)
        assert http_schema in [None, 'http', 'https'], repr(http_schema)
        self.name = name
        self.setup = setup  # setup as local system if setup_cloud_account is None, to cloud if it is set
        # By default, by Server's 'init' method  it's hardcoded cloud host will be patched/restored to the one
        # deduced from --cloud-group option. With leave_initial_cloud_host=True, this step will be skipped.
        # With leave_initial_cloud_host=True VM will also be always recreated before this test to ensure
        # server binaries has original cloud host encoded by compilation step.
        self.leave_initial_cloud_host = leave_initial_cloud_host  # bool
        self.vm = vm  # VagrantVM or None
        self.config_file_params = config_file_params  # dict or None
        self.setup_settings = setup_settings or {}  # dict
        self.setup_cloud_account = setup_cloud_account  # CloudAccount or None
        self.http_schema = http_schema or DEFAULT_HTTP_SCHEMA  # 'http' or 'https'
        self.rest_api_timeout = rest_api_timeout

    def __repr__(self):
        return 'ServerConfig(%r @ %s)' % (self.name, self.vm)


class Server(object):
    """Mediaserver, same for physical and virtual machines"""

    def __init__(self, name, service, installation, api, machine, port=None):
        assert name, repr(name)
        self.title = name.upper()
        self.name = '%s-%s' % (name, str(uuid.uuid4())[-12:])
        self.os_access = machine.os_access  # Deprecated.
        self.installation = installation
        self.service = service
        self.machine = machine
        self.rest_api = api
        self.internal_ip_port = port or MEDIASERVER_LISTEN_PORT
        self.forwarded_port = None

    def __repr__(self):
        return '<Server at %s>' % self.rest_api.url

    @property
    def user(self):
        return self.rest_api.user

    @property
    def password(self):
        return self.rest_api.password

    @property
    def dir(self):
        return self.installation.dir

    def list_core_files(self):
        return self.installation.list_core_files()

    def is_online(self):
        try:
            self.rest_api.get('/api/ping')
        except requests.RequestException:
            return False
        else:
            return True

    def start(self, already_started_ok=False):
        if self.service.is_running():
            if not already_started_ok:
                raise Exception("Already started")
        else:
            self.service.start()
            wait_until(self.is_online)

    def stop(self, already_stopped_ok=False):
        if self.service.is_running():
            self.service.stop()
            wait_succeeded = wait_until(lambda: not self.service.is_running(), name="until stopped")
            if not wait_succeeded:
                raise Exception("Cannot wait for server to stop")
        else:
            if not already_stopped_ok:
                raise Exception("Already stopped")

    def get_log_file(self):
        return self.installation.get_log_file()

    def restart_via_api(self, timeout=MEDIASERVER_START_TIMEOUT):
        old_runtime_id = self.rest_api.api.moduleInformation.GET()['runtimeId']
        log.info("Runtime id before restart: %s", old_runtime_id)
        started_at = datetime.datetime.now(pytz.utc)
        self.rest_api.api.restart.GET()
        sleep_time_sec = timeout.total_seconds() / 100.
        failed_connections = 0
        while True:
            try:
                response = self.rest_api.api.moduleInformation.GET()
            except requests.ConnectionError as e:
                if datetime.datetime.now(pytz.utc) - started_at > timeout:
                    assert False, "Server hasn't started, caught %r, timed out." % e
                log.debug("Expected failed connection: %r", e)
                failed_connections += 1
                time.sleep(sleep_time_sec)
                continue
            new_runtime_id = response['runtimeId']
            if new_runtime_id == old_runtime_id:
                if failed_connections > 0:
                    assert False, "Runtime id remains same after failed connections."
                if datetime.datetime.now(pytz.utc) - started_at > timeout:
                    assert False, "Server hasn't stopped, timed out."
                log.warning("Server hasn't stopped yet, delay is acceptable.")
                time.sleep(sleep_time_sec)
                continue
            log.info("Server restarted successfully, new runtime id is %s", new_runtime_id)
            break

    def make_core_dump(self):
        self.service.make_core_dump()

    def get_time(self):
        started_at = datetime.datetime.now(pytz.utc)
        time_response = self.rest_api.ec2.getCurrentTime.GET()
        received = datetime.datetime.fromtimestamp(float(time_response['value']) / 1000., pytz.utc)
        return RunningTime(received, datetime.datetime.now(pytz.utc) - started_at)

    def patch_binary_set_cloud_host(self, new_host):
        assert not self.service.is_running(), 'Server %s must be stopped first for patching its binaries' % self
        self.installation.patch_binary_set_cloud_host(new_host)
        self.set_user_password(REST_API_USER, REST_API_PASSWORD)  # Must be reset to default ones

    def set_system_settings(self, **kw):
        self.rest_api.api.systemSettings.GET(**kw)

    def setup_local_system(self, **kw):
        log.info('Setting up server as local system %s:', self)
        self.rest_api.api.setupLocalSystem.POST(systemName=self.name, password=REST_API_PASSWORD, **kw)  # leave password unchanged

    def setup_cloud_system(self, cloud_account, **kw):
        assert isinstance(cloud_account, CloudAccount), repr(cloud_account)
        log.info('Setting up server as local system %s:', self)
        bind_info = cloud_account.bind_system(self.name)
        setup_response = self.rest_api.api.setupCloudSystem.POST(
            systemName=self.name,
            cloudAuthKey=bind_info.auth_key,
            cloudSystemID=bind_info.system_id,
            cloudAccountName=cloud_account.user,
            timeout=datetime.timedelta(minutes=5),
            **kw)
        settings = setup_response['settings']
        self.set_user_password(cloud_account.user, cloud_account.password)
        assert get_settings(self.rest_api)['systemName'] == self.name
        return settings

    def detach_from_cloud(self, password):
        self.rest_api.api.detachFromCloud.POST(password=password)
        self.set_user_password(REST_API_USER, password)

    def merge(self, other_server_list):
        assert is_list_inst(other_server_list, Server), repr(other_server_list)
        for server in other_server_list:
            self.merge_systems(server)

    def merge_systems(self, other, remote_network=IPNetwork('10.254.0.0/16'), take_remote_settings=False):
        log.info('Merging servers: %s with local_system_id=%r and %s', self, other)
        assert self.is_online() and other.is_online()
        assert get_local_system_id(self.rest_api) != get_local_system_id(other.rest_api)  # Must not be merged yet.

        realm, nonce = other.get_nonce()

        def make_key(method):
            digest = generate_auth_key(method, other.user.lower(), other.password, nonce, realm)
            return urllib.quote(base64.urlsafe_b64encode(':'.join([other.user.lower(), nonce, digest])))
        interfaces = other.rest_api.api.iflist.GET()
        url = None
        for interface in interfaces:
            ip_address_to = IPAddress(interface['ipAddr'])
            if ip_address_to in remote_network:
                url = 'http://{ip_address}:{port}/'.format(ip_address=ip_address_to, port=self.internal_ip_port)
        if url is None:
            raise Exception("No IP address from {} from interfaces:\n{}".format(remote_network, pformat(interfaces)))
        self.rest_api.api.mergeSystems.POST(
            url=url,
            getKey=make_key('GET'), postKey=make_key('POST'),
            takeRemoteSettings=take_remote_settings, mergeOneServer=False)
        if take_remote_settings:
            self.set_user_password(other.user, other.password)
        else:
            other.set_user_password(self.user, self.password)
        wait_until(lambda: get_local_system_id(self.rest_api) == get_local_system_id(other.rest_api))

    def set_user_password(self, user, password):
        log.debug('%s got user/password: %r/%r', self, user, password)
        self.rest_api.set_credentials(user, password)
        if self.is_online():
            self._wait_for_credentials_accepted()

    # wait until existing server credentials become valid
    def _wait_for_credentials_accepted(self):
        start_time = datetime_utc_now()
        t = time.time()
        while datetime_utc_now() - start_time < MEDIASERVER_CREDENTIALS_TIMEOUT:
            try:
                self.rest_api.ec2.testConnection.GET()
                log.info('Server accepted new credentials in %s', datetime_utc_now() - start_time)
                return
            except HttpError as x:
                if x.status_code != 401:
                    raise
            time.sleep(1)
        pytest.fail('Timed out in %s waiting for server %s to accept credentials: user=%r, password=%r'
                    % (MEDIASERVER_CREDENTIALS_TIMEOUT, self, self.user, self.password))

    def get_nonce(self):
        response = self.rest_api.api.getNonce.GET()
        return response['realm'], response['nonce']

    def add_camera(self, camera):
        assert not camera.id, 'Already added to a server with id %r' % camera.id
        params = camera.get_info(parent_id=get_server_id(self.rest_api))
        result = self.rest_api.ec2.saveCamera.POST(**params)
        camera.id = result['id']
        return camera.id

    def set_camera_recording(self, camera, recording):
        assert camera, 'Camera %r is not yet registered on server' % camera
        schedule_tasks = [make_schedule_task(day_of_week + 1) for day_of_week in range(7)]
        self.rest_api.ec2.saveCameraUserAttributes.POST(
            cameraId=camera.id, scheduleEnabled=recording, scheduleTasks=schedule_tasks)

    def start_recording_camera(self, camera):
        self.set_camera_recording(camera, recording=True)

    def stop_recording_camera(self, camera):
        self.set_camera_recording(camera, recording=False)

    @property
    def storage(self):
        # GET /ec2/getStorages is not always possible: server sometimes is not started.
        storage_path = self.installation.dir / MEDIASERVER_STORAGE_PATH
        return Storage(self.os_access, storage_path, self.os_access.get_timezone())

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
                              datetime.timedelta(seconds=int(d['durationMs']) / 1000.))
                   for d in self.rest_api.ec2.recordedTimePeriods.GET(cameraId=camera.id, flat=True)]
        log.info('Server %r returned %d recorded periods:', self.name, len(periods))
        for period in periods:
            log.info('\t%s', period)
        return periods

    def get_media_stream(self, stream_type, camera):
        assert stream_type in ['rtsp', 'webm', 'hls', 'direct-hls'], repr(stream_type)
        assert isinstance(camera, Camera), repr(camera)
        return open_media_stream(self.rest_api.url, self.user, self.password, stream_type, camera.mac_addr)


class Storage(object):

    def __init__(self, os_access, dir, timezone=None):
        self.os_access = os_access
        self.dir = dir
        self.timezone = timezone or os_access.get_timezone()

    def save_media_sample(self, camera, start_time, sample):
        assert isinstance(camera, Camera), repr(camera)
        assert isinstance(start_time, datetime.datetime) and start_time.tzinfo, repr(start_time)
        assert start_time.tzinfo  # naive datetime are forbidden, use pytz.utc or tzlocal.get_localtimezone() for tz
        assert isinstance(sample, SampleMediaFile), repr(sample)
        camera_mac_addr = camera.mac_addr
        unixtime_utc_ms = int(datetime_utc_to_timestamp(start_time) * 1000)

        contents = self._read_with_start_time_metadata(sample, unixtime_utc_ms)

        lowq_fpath = self._construct_fpath(camera_mac_addr, 'low_quality', start_time, unixtime_utc_ms, sample.duration)
        hiq_fpath = self._construct_fpath(camera_mac_addr, 'hi_quality',  start_time, unixtime_utc_ms, sample.duration)

        log.info('Storing media sample %r to %r', sample.fpath, lowq_fpath)
        self.os_access.write_file(lowq_fpath, contents)
        log.info('Storing media sample %r to %r', sample.fpath, hiq_fpath)
        self.os_access.write_file(hiq_fpath, contents)

    def _read_with_start_time_metadata(self, sample, unixtime_utc_ms):
        _, path = tempfile.mkstemp(suffix=sample.fpath.suffix)
        path = Path(path)
        try:
            LocalAccess().run_command([
                'ffmpeg',
                '-i', sample.fpath,
                '-codec', 'copy',
                '-metadata', 'START_TIME=%s' % unixtime_utc_ms,
                '-y',
                path])
            return path.read_bytes()
        finally:
            path.unlink()

    # server stores media data in this format, using local time for directory parts:
    # <data dir>/<{hi_quality,low_quality}>/<camera-mac>/<year>/<month>/<day>/<hour>/<start,unix timestamp ms>_<duration,ms>.mkv
    # for example:
    # server/var/data/data/low_quality/urn_uuid_b0e78864-c021-11d3-a482-f12907312681/2017/01/27/12/1485511093576_21332.mkv
    def _construct_fpath(self, camera_mac_addr, quality_part, start_time, unixtime_utc_ms, duration):
        local_dt = start_time.astimezone(self.timezone)  # Local to VM.
        duration_ms = int(duration.total_seconds() * 1000)
        return self.dir.joinpath(
            quality_part, camera_mac_addr,
            '%02d' % local_dt.year, '%02d' % local_dt.month, '%02d' % local_dt.day, '%02d' % local_dt.hour,
            '%s_%s.mkv' % (unixtime_utc_ms, duration_ms))
