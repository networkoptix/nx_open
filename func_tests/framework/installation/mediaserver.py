"""Work with mediaserver as single entity: start/stop, setup, configure, access HTTP api, storage, etc..."""

import datetime
import logging
import tempfile

import pytz
from pathlib2 import Path

from framework.camera import Camera, SampleMediaFile
from framework.installation.installation import Installation
from framework.mediaserver_api import GenericMediaserverApi, MediaserverApi
from framework.method_caching import cached_property
from framework.os_access.local_shell import local_shell
from framework.utils import datetime_utc_to_timestamp
from framework.waiting import wait_for_true

DEFAULT_HTTP_SCHEMA = 'http'

MEDIASERVER_STORAGE_PATH = 'var/data'

MEDIASERVER_CREDENTIALS_TIMEOUT = datetime.timedelta(minutes=5)
MEDIASERVER_MERGE_TIMEOUT = MEDIASERVER_CREDENTIALS_TIMEOUT  # timeout for local system ids become the same
MEDIASERVER_MERGE_REQUEST_TIMEOUT = datetime.timedelta(seconds=90)  # timeout for mergeSystems REST api request
MEDIASERVER_START_TIMEOUT = datetime.timedelta(minutes=2)  # timeout when waiting for server become online (pingable)

_logger = logging.getLogger(__name__)


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
        self.api = MediaserverApi(GenericMediaserverApi.new(name, forwarded_address, forwarded_port))

    def __repr__(self):
        return '<Mediaserver {} at {}>'.format(self.name, self.api.generic.http.url(''))

    def start(self, already_started_ok=False):
        if self.service.is_running():
            if not already_started_ok:
                raise Exception("Already started")
        else:
            self.service.start()
            wait_for_true(self.api.is_online, timeout_sec=MEDIASERVER_START_TIMEOUT.total_seconds())

    def stop(self, already_stopped_ok=False):
        _logger.info("Stop mediaserver %r.", self)
        if self.service.is_running():
            self.service.stop()
            wait_for_true(lambda: not self.service.is_running(), "{} stops".format(self.service))
        else:
            if not already_stopped_ok:
                raise Exception("Already stopped")

    @property
    def storage(self):
        # GET /ec2/getStorages is not always possible: server sometimes is not started.
        storage_path = self.installation.dir / MEDIASERVER_STORAGE_PATH
        return Storage(self.os_access, storage_path)


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
