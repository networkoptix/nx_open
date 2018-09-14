import logging
import time
from abc import ABCMeta, abstractmethod
from contextlib import contextmanager
from datetime import datetime

from framework.os_access.command import Command
from framework.os_access.path import FileSystemPath, copy_file

_logger = logging.getLogger(__name__)

DEFAULT_SIZE_LIMIT_BYTES = 512 * 1024 * 1024
DEFAULT_DURATION_LIMIT_SEC = 15 * 60


class TrafficCapture(object):
    __metaclass__ = ABCMeta

    def __init__(self, dir):
        self._dir = dir  # type: FileSystemPath

    @abstractmethod
    def _make_capturing_command(self, capture_path, size_limit_bytes, duration_limit_sec):
        return Command()

    @contextmanager
    def capturing(
            self,
            local_capture_file,  # type: FileSystemPath
            size_limit_bytes=DEFAULT_SIZE_LIMIT_BYTES,
            duration_limit_sec=DEFAULT_DURATION_LIMIT_SEC,
            ):
        self._dir.mkdir(exist_ok=True, parents=True)
        old_capture_files = sorted(self._dir.glob('*'))
        for old_capture_file in old_capture_files[:-2]:
            old_capture_file.unlink()
        remote_capture_file = self._dir / '{:%Y%m%d%H%M%S%u}.cap'.format(datetime.utcnow())
        _logger.info('Start capturing traffic to file %s', remote_capture_file)
        with self._make_capturing_command(remote_capture_file, size_limit_bytes, duration_limit_sec).running() as run:
            time.sleep(1)
            try:
                yield remote_capture_file
            finally:
                time.sleep(1)
                run.terminate()
                _logger.info('Stop capturing traffic to file %s', remote_capture_file)
                try:
                    stdout, stderr = run.communicate(timeout_sec=30)  # Time to cleanup.
                finally:
                    _logger.debug("Outcome: %s", run.outcome)
                _logger.debug("STDOUT:\n%s", stdout)
                _logger.debug("STDERR:\n%s", stderr)
                copy_file(remote_capture_file, local_capture_file)
