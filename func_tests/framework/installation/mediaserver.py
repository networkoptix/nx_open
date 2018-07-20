"""Work with mediaserver as single entity: start/stop, setup, configure, access HTTP api, storage, etc..."""

import datetime
import json
import logging
import tempfile
import time
from Crypto.Cipher import AES

import pytz
import requests.exceptions
from pathlib2 import Path

from framework.api_shortcuts import get_server_id
from framework.camera import Camera, SampleMediaFile, make_schedule_task
from framework.installation.installation import Installation
from framework.media_stream import open_media_stream
from framework.method_caching import cached_property
from framework.os_access.local_shell import local_shell
from framework.rest_api import GenericMediaserverApi
from framework.utils import datetime_utc_to_timestamp
from framework.waiting import wait_for_true

DEFAULT_HTTP_SCHEMA = 'http'

MEDIASERVER_STORAGE_PATH = 'var/data'

MEDIASERVER_CREDENTIALS_TIMEOUT = datetime.timedelta(minutes=5)
MEDIASERVER_MERGE_TIMEOUT = MEDIASERVER_CREDENTIALS_TIMEOUT  # timeout for local system ids become the same
MEDIASERVER_MERGE_REQUEST_TIMEOUT = datetime.timedelta(seconds=90)  # timeout for mergeSystems REST api request
MEDIASERVER_START_TIMEOUT = datetime.timedelta(minutes=2)  # timeout when waiting for server become online (pingable)

_logger = logging.getLogger(__name__)


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


def parse_json_fields(data):
    if isinstance(data, dict):
        return {k: parse_json_fields(v) for k, v in data.iteritems()}
    if isinstance(data, list):
        return [parse_json_fields(i) for i in data]
    if isinstance(data, basestring):
        try:
            json_data = json.loads(data)
        except ValueError:
            pass
        else:
            return parse_json_fields(json_data)
    return data


def encode_camera_credentials(login, password):
    # Do not try to understand this code, this is hardcoded the same way as in common library.
    data = ':'.join([login, password])
    data += chr(0) * (16 - (len(data) % 16))
    key = '4453D6654C634636990B2E5AA69A1312'.decode('hex')
    iv = '000102030405060708090a0b0c0d0e0f'.decode('hex')
    return AES.new(key, AES.MODE_CBC, iv).encrypt(data).encode('hex')


class Mediaserver(object):
    """Mediaserver, same for physical and virtual machines"""

    def __init__(self, name, installation, port=7001):  # type: (str, Installation) -> None
        assert port is not None
        self.name = name
        self.installation = installation
        self.service = installation.service
        self.os_access = installation.os_access
        self.port = port
        forwarded_port = installation.os_access.port_map.remote.tcp(self.port)
        forwarded_address = installation.os_access.port_map.remote.address
        self.api = GenericMediaserverApi.new(name, forwarded_address, forwarded_port)

    def __repr__(self):
        return '<Mediaserver {} at {}>'.format(self.name, self.api.http.url(''))

    def is_online(self):
        try:
            self.api.get('/api/ping')
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
            wait_for_true(self.is_online, timeout_sec=MEDIASERVER_START_TIMEOUT.total_seconds())

    def stop(self, already_stopped_ok=False):
        _logger.info("Stop mediaserver %r.", self)
        if self.service.is_running():
            self.service.stop()
            wait_for_true(lambda: not self.service.is_running(), "{} stops".format(self.service))
        else:
            if not already_stopped_ok:
                raise Exception("Already stopped")

    def restart_via_api(self, timeout=MEDIASERVER_START_TIMEOUT):
        old_runtime_id = self.api.get('api/moduleInformation')['runtimeId']
        _logger.info("Runtime id before restart: %s", old_runtime_id)
        started_at = datetime.datetime.now(pytz.utc)
        self.api.get('api/restart')
        sleep_time_sec = timeout.total_seconds() / 100.
        failed_connections = 0
        while True:
            try:
                response = self.api.get('api/moduleInformation')
            except requests.ConnectionError as e:
                if datetime.datetime.now(pytz.utc) - started_at > timeout:
                    assert False, "Mediaserver hasn't started, caught %r, timed out." % e
                _logger.debug("Expected failed connection: %r", e)
                failed_connections += 1
                time.sleep(sleep_time_sec)
                continue
            new_runtime_id = response['runtimeId']
            if new_runtime_id == old_runtime_id:
                if failed_connections > 0:
                    assert False, "Runtime id remains same after failed connections."
                if datetime.datetime.now(pytz.utc) - started_at > timeout:
                    assert False, "Mediaserver hasn't stopped, timed out."
                _logger.warning("Mediaserver hasn't stopped yet, delay is acceptable.")
                time.sleep(sleep_time_sec)
                continue
            _logger.info("Mediaserver restarted successfully, new runtime id is %s", new_runtime_id)
            break

    def add_camera(self, camera):
        assert not camera.id, 'Already added to a server with id %r' % camera.id
        params = camera.get_info(parent_id=get_server_id(self.api))
        result = self.api.post('ec2/saveCamera', dict(**params))
        camera.id = result['id']
        return camera.id

    def set_camera_recording(self, camera, recording, options={}):
        assert camera, 'Camera %r is not yet registered on server' % camera
        schedule_tasks = [make_schedule_task(day_of_week + 1) for day_of_week in range(7)]
        for task in schedule_tasks:
            task.update(options)
        self.api.post('ec2/saveCameraUserAttributes', dict(
            cameraId=camera.id, scheduleEnabled=recording, scheduleTasks=schedule_tasks))

    def start_recording_camera(self, camera, options={}):
        self.set_camera_recording(camera, recording=True, options=options)

    def stop_recording_camera(self, camera, options={}):
        self.set_camera_recording(camera, recording=False, options=options)

    @property
    def storage(self):
        # GET /ec2/getStorages is not always possible: server sometimes is not started.
        storage_path = self.installation.dir / MEDIASERVER_STORAGE_PATH
        return Storage(self.os_access, storage_path)

    def rebuild_archive(self):
        self.api.get('api/rebuildArchive', params=dict(mainPool=1, action='start'))
        for i in range(30):
            response = self.api.get('api/rebuildArchive', params=dict(mainPool=1))
            if response['state'] == 'RebuildState_None':
                return
            time.sleep(0.3)
        assert False, 'Timed out waiting for archive to rebuild'

    def get_recorded_time_periods(self, camera):
        assert camera.id, 'Camera %r is not yet registered on server' % camera.name
        periods = [
            TimePeriod(datetime.datetime.utcfromtimestamp(int(d['startTimeMs']) / 1000.).replace(tzinfo=pytz.utc),
                       datetime.timedelta(seconds=int(d['durationMs']) / 1000.))
            for d in self.api.get('ec2/recordedTimePeriods', params=dict(cameraId=camera.id, flat=True))]
        _logger.info('Mediaserver %r returned %d recorded periods:', self.name, len(periods))
        for period in periods:
            _logger.info('\t%s', period)
        return periods

    def get_media_stream(self, stream_type, camera):
        assert stream_type in ['rtsp', 'webm', 'hls', 'direct-hls'], repr(stream_type)
        assert isinstance(camera, Camera), repr(camera)
        return open_media_stream(self.api.http.url(''), self.api.http.user, self.api.http.password, stream_type, camera.mac_addr)

    def get_resources(self, path, *args, **kwargs):
        resources = self.api.get('ec2/get' + path, *args, **kwargs)
        for resource in resources:
            for p in resource.pop('addParams', []):
                resource[p['name']] = p['value']
        return parse_json_fields(resources)

    def get_resource(self, path, id, **kwargs):
        resources = self.get_resources(path, params=dict(id=id))
        assert len(resources) == 1
        return resources[0]

    def set_camera_credentials(self, id, login, password):
        c = encode_camera_credentials(login, password)
        self.api.post("ec2/setResourceParams", [dict(resourceId=id, name='credentials', value=c)])


class Storage(object):

    def __init__(self, os_access, dir):
        self.os_access = os_access
        self.dir = dir

    @cached_property  # TODO: Use cached_getter.
    def timezone(self):
        tzname = self.os_access.Path('/etc/timezone').read_text().strip()
        return pytz.timezone(tzname)

    def save_media_sample(self, camera, start_time, sample):
        assert isinstance(camera, Camera), repr(camera)
        assert isinstance(start_time, datetime.datetime) and start_time.tzinfo, repr(start_time)
        assert start_time.tzinfo  # naive datetime are forbidden, use pytz.utc or tzlocal.get_localtimezone() for tz
        assert isinstance(sample, SampleMediaFile), repr(sample)
        camera_mac_addr = camera.mac_addr
        unixtime_utc_ms = int(datetime_utc_to_timestamp(start_time) * 1000)

        contents = self._read_with_start_time_metadata(sample, unixtime_utc_ms)

        for quality in {'low_quality', 'hi_quality'}:
            path = self._construct_fpath(camera_mac_addr, quality, start_time, unixtime_utc_ms, sample.duration)
            path.parent.mkdir(parents=True, exist_ok=True)
            _logger.info('Storing media sample %r to %r', sample.fpath, path)
            path.write_bytes(contents)

    def _read_with_start_time_metadata(self, sample, unixtime_utc_ms):
        _, path = tempfile.mkstemp(suffix=sample.fpath.suffix)
        path = Path(path)
        try:
            local_shell.run_command([
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
    # <data dir>/
    #   <{hi_quality,low_quality}>/<camera-mac>/
    #     <year>/<month>/<day>/<hour>/
    #       <start,unix timestamp ms>_<duration,ms>.mkv
    # for example:
    # server/var/data/data/
    #   low_quality/urn_uuid_b0e78864-c021-11d3-a482-f12907312681/
    #     2017/01/27/12/1485511093576_21332.mkv
    def _construct_fpath(self, camera_mac_addr, quality_part, start_time, unixtime_utc_ms, duration):
        local_dt = start_time.astimezone(self.timezone)  # Local to Machine.
        duration_ms = int(duration.total_seconds() * 1000)
        return self.dir.joinpath(
            quality_part, camera_mac_addr,
            '%02d' % local_dt.year, '%02d' % local_dt.month, '%02d' % local_dt.day, '%02d' % local_dt.hour,
            '%s_%s.mkv' % (unixtime_utc_ms, duration_ms))
