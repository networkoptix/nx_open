"""Work with mediaserver as single entity: start/stop, setup, configure, access HTTP api, storage, etc..."""

import datetime
import logging
import tempfile
from abc import ABCMeta, abstractmethod

import pytz
from pathlib2 import Path

from framework.camera import Camera, SampleMediaFile
from framework.installation.installation import Installation
from framework.installation.installer import InstallerSet
from framework.installation.make_installation import make_installation
from framework.mediaserver_api import GenericMediaserverApi, MediaserverApi
from framework.method_caching import cached_property
from framework.os_access.local_shell import local_shell
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import copy_file
from ..switched_logging import with_logger
from framework.utils import datetime_utc_to_timestamp
from framework.waiting import wait_for_true

DEFAULT_HTTP_SCHEMA = 'http'

MEDIASERVER_STORAGE_PATH = 'var/data'

MEDIASERVER_CREDENTIALS_TIMEOUT = datetime.timedelta(minutes=5)
MEDIASERVER_MERGE_TIMEOUT = MEDIASERVER_CREDENTIALS_TIMEOUT  # timeout for local system ids become the same
MEDIASERVER_MERGE_REQUEST_TIMEOUT = datetime.timedelta(seconds=90)  # timeout for mergeSystems REST api request
MEDIASERVER_START_TIMEOUT = datetime.timedelta(minutes=2)  # timeout when waiting for server become online (pingable)

_logger = logging.getLogger(__name__)


@with_logger(_logger, 'framework.waiting')
@with_logger(_logger, 'framework.http_api')
@with_logger(_logger, 'framework.mediaserver_api')
class BaseMediaserver(object):
    __metaclass__ = ABCMeta

    def __init__(self, name, installation):  # type: (str, Installation) -> None
        self.name = name
        self.installation = installation

    @abstractmethod
    def is_online(self):
        pass

    def start(self, already_started_ok=False):
        _logger.info('Start %s', self)
        service = self.installation.service
        if service.is_running():
            if not already_started_ok:
                raise Exception("Already started")
        else:
            service.start()
            wait_for_true(
                self.is_online,
                description='{} is started'.format(self),
                timeout_sec=MEDIASERVER_START_TIMEOUT.total_seconds(),
                )

    def stop(self, already_stopped_ok=False):
        _logger.info("Stop mediaserver %s.", self)
        service = self.installation.service
        if service.is_running():
            service.stop()
            wait_for_true(lambda: not service.is_running(), "{} stops".format(service))
        else:
            if not already_stopped_ok:
                raise Exception("Already stopped")

    def examine(self, stopped_ok=False):
        examination_logger = _logger.getChild('examination')
        examination_logger.info('Post-test check for %s', self)
        status = self.installation.service.status()
        if status.is_running:
            examination_logger.debug("%s is running.", self)
            if self.is_online():
                examination_logger.debug("%s is online.", self)
            else:
                self.installation.os_access.make_core_dump(status.pid)
                _logger.error('{} is not online; core dump made.'.format(self))
        else:
            if stopped_ok:
                examination_logger.info("%s is stopped; it's OK.", self)
            else:
                _logger.error("{} is stopped.".format(self))

    def collect_artifacts(self, artifacts_dir):
        for file in self.installation.list_log_files():
            if file.exists():
                copy_file(file, artifacts_dir / file.name)
        for core_dump in self.installation.list_core_dumps():
            local_core_dump_path = artifacts_dir / core_dump.name
            copy_file(core_dump, local_core_dump_path)
            # noinspection PyBroadException
            try:
                traceback = self.installation.parse_core_dump(core_dump)
                backtrace_name = local_core_dump_path.name + '.backtrace.txt'
                local_traceback_path = local_core_dump_path.with_name(backtrace_name)
                local_traceback_path.write_text(traceback)
            except Exception:
                _logger.exception("Cannot parse core dump: %s.", core_dump)


class Mediaserver(BaseMediaserver):
    """Mediaserver, same for physical and virtual machines"""

    def __init__(self, name, installation, port=7001):  # type: (str, Installation, int) -> None
        super(Mediaserver, self).__init__(name, installation)
        assert port is not None
        self.name = name
        self.os_access = installation.os_access
        self.port = port
        self.service = installation.service
        forwarded_port = installation.os_access.port_map.remote.tcp(self.port)
        forwarded_address = installation.os_access.port_map.remote.address
        self.api = MediaserverApi(GenericMediaserverApi.new(name, forwarded_address, forwarded_port))

    def __str__(self):
        return 'Mediaserver {} at {}'.format(self.name, self.api.generic.http.url(''))

    def __repr__(self):
        return '<{!s}>'.format(self)

    @classmethod
    def setup(cls, os_access, installer_set, ssl_key_cert):
        # type: (OSAccess, InstallerSet, str) -> Mediaserver
        """Get mediaserver as if it hasn't run before."""
        installation = make_installation(os_access, installer_set.customization)
        installer = installation.choose_installer(installer_set.installers)
        installation.install(installer)
        mediaserver = cls(os_access.alias, installation)
        mediaserver.stop(already_stopped_ok=True)
        mediaserver.installation.cleanup(ssl_key_cert)
        return mediaserver

    def is_online(self):
        return self.api.is_online()

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
            _logger.info('Storing media sample %r to %r', sample.path, path)
            path.write_bytes(contents)

    def _read_with_start_time_metadata(self, sample, unixtime_utc_ms):
        _, path = tempfile.mkstemp(suffix=sample.path.suffix)
        path = Path(path)
        try:
            local_shell.run_command([
                'ffmpeg',
                '-i', sample.path,
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
