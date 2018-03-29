import logging

from pathlib2 import PurePosixPath

logger = logging.getLogger(__name__)


class MoveLock(object):
    class AlreadyAcquired(Exception):
        pass

    class NotAcquired(Exception):
        pass

    def __init__(self, os_access, path):
        self._os_access = os_access
        self._path = path

    def __enter__(self):
        logger.info("Acquire lock at %r", self._path)
        temp_path = PurePosixPath(self._os_access.run_command(['mktemp']).strip())
        self._os_access.run_command(['mv', '-n', temp_path, self._path])
        if self._os_access.file_exists(temp_path):
            self._os_access.run_command(['rm', temp_path])
            raise self.AlreadyAcquired()

    def __exit__(self, exc_type, exc_val, exc_tb):
        if not self._os_access.file_exists(self._path):
            raise self.NotAcquired()
        logger.info("Release lock at %r", self._path)
        self._os_access.run_command(['rm', self._path])

