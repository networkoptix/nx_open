import logging
import time
from abc import ABCMeta, abstractmethod
from contextlib import contextmanager
from datetime import datetime

from framework.os_access.command import Command
from framework.os_access.path import FileSystemPath

_logger = logging.getLogger(__name__)

DEFAULT_SIZE_LIMIT_BYTES = 512 * 1024 * 1024
DEFAULT_DURATION_LIMIT_SEC = 15 * 60


class TrafficCapture(object):
    __metaclass__ = ABCMeta

    def __init__(self, dir):
        self._dir = dir  # type: FileSystemPath
        self._dir.mkdir(exist_ok=True, parents=True)

    @abstractmethod
    def _make_capturing_command(self, capture_path, size_limit_bytes, duration_limit_sec):
        return Command()

    @contextmanager
    def capturing(self, size_limit_bytes=DEFAULT_SIZE_LIMIT_BYTES, duration_limit_sec=DEFAULT_DURATION_LIMIT_SEC):
        old_capture_files = sorted(self._dir.glob('*'))
        for old_capture_file in old_capture_files[:-2]:
            old_capture_file.unlink()
        capture_file = self._dir / '{:%Y%m%d%H%M%S%u}.cap'.format(datetime.utcnow())
        with self._make_capturing_command(capture_file, size_limit_bytes, duration_limit_sec).running() as run:
            time.sleep(1)
            try:
                yield capture_file
            finally:
                time.sleep(1)
                run.terminate()
                stdout, stderr = run.communicate(timeout_sec=5)  # Time to cleanup.
                _logger.debug("Outcome: %s", run.outcome)
                _logger.debug("STDOUT:\n%s", stdout)
                _logger.debug("STDERR:\n%s", stderr)
