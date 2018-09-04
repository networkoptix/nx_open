import errno
import logging
from abc import ABCMeta, abstractmethod
from contextlib import contextmanager

import portalocker

from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.local_path import LocalPath
from framework.os_access.posix_shell import Shell

_logger = logging.getLogger(__name__)


class AlreadyAcquired(Exception):
    pass


class NotAcquired(Exception):
    pass


class Lock(object):
    __metaclass__ = ABCMeta

    @abstractmethod
    def acquired(self, timeout_sec=10):
        pass


class PosixShellFileLock(Lock):
    def __init__(self, shell, path):
        self._shell = shell  # type: Shell
        self._path = path  # type: LocalPath

    def __repr__(self):
        return '<PosixShellFileLock at {} via {}>'.format(self._path, self._shell)

    @contextmanager
    def acquired(self, timeout_sec=10):
        command = self._shell.sh_script(
            # language=Bash
            '''
                exec 3>"$FILE"  # Open file as file descriptor 3 (small arbitrary integer).
                flock --wait $TIMEOUT_SEC 3 || exit 3
                echo 'acquired'
                read -r _ # Wait for any input to continue.
                # No need to unlock: file is closed.
                ''',
            env={'FILE': self._path, 'TIMEOUT_SEC': timeout_sec},
            )
        with command.running() as run:
            try:
                run.expect(b'acquired\n', timeout_sec=timeout_sec + 1)
            except exit_status_error_cls(3):
                raise AlreadyAcquired("{} already acquired, timeout {}".format(self, timeout_sec))
            try:
                yield
            finally:
                run.communicate(input=b'release\n', timeout_sec=1)


class PortalockerLock(Lock):
    def __init__(self, path):
        self._path = path

    @contextmanager
    def acquired(self, timeout_sec=10):
        try:
            with portalocker.utils.Lock(str(self._path), timeout=timeout_sec):
                yield
        except portalocker.exceptions.LockException as e:
            while not isinstance(e, IOError):
                e, = e.args
            if e.errno != errno.EAGAIN:
                raise e
            raise AlreadyAcquired()
