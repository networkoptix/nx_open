import datetime
import logging
import tempfile

from pathlib2 import Path

from framework.camera import Camera, SampleMediaFile
from framework.os_access.local_shell import local_shell
from framework.os_access.os_access_interface import OSAccess
from framework.utils import datetime_utc_to_timestamp

_logger = logging.getLogger(__name__)


class Storage(object):

    def __init__(self, os_access, dir):
        self.os_access = os_access  # type: OSAccess
        self.dir = dir

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
        local_dt = start_time.astimezone(self.os_access.time.get_tz())  # Local to Machine.
        duration_ms = int(duration.total_seconds() * 1000)
        return self.dir.joinpath(
            quality_part, camera_mac_addr,
            '%02d' % local_dt.year, '%02d' % local_dt.month, '%02d' % local_dt.day, '%02d' % local_dt.hour,
            '%s_%s.mkv' % (unixtime_utc_ms, duration_ms))
