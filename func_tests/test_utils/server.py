"""Work with mediaserver as single entity: start/stop, setup, configure, access HTTP api, storage, etc..."""

import datetime
import logging
import tempfile
import time
import uuid

import pytz
import requests.exceptions
from pathlib2 import Path

from test_utils.api_shortcuts import get_server_id
from test_utils.utils import wait_until
from .camera import Camera, SampleMediaFile, make_schedule_task
from .media_stream import open_media_stream
from .os_access import LocalAccess
from .utils import datetime_utc_to_timestamp

DEFAULT_HTTP_SCHEMA = 'http'

MEDIASERVER_STORAGE_PATH = 'var/data'

MEDIASERVER_CREDENTIALS_TIMEOUT = datetime.timedelta(minutes=5)
MEDIASERVER_MERGE_TIMEOUT = MEDIASERVER_CREDENTIALS_TIMEOUT  # timeout for local system ids become the same
MEDIASERVER_MERGE_REQUEST_TIMEOUT = datetime.timedelta(seconds=90)  # timeout for mergeSystems REST api request
MEDIASERVER_START_TIMEOUT = datetime.timedelta(minutes=5)  # timeout when waiting for server become online (pingable)


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


class Server(object):
    """Mediaserver, same for physical and virtual machines"""

    def __init__(self, name, service, installation, api, machine, port):
        assert name, repr(name)
        self.title = name.upper()
        self.name = '%s-%s' % (name, str(uuid.uuid4())[-12:])
        self.os_access = machine.os_access  # Deprecated.
        self.installation = installation
        self.service = service
        self.machine = machine
        self.rest_api = api
        self.port = port

    def __repr__(self):
        return '<Server {} at {}>'.format(self.name, self.rest_api.url(''))

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
        return open_media_stream(self.rest_api.url(''), self.rest_api.user, self.rest_api.password, stream_type, camera.mac_addr)


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
