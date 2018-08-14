import errno
import fcntl
import logging
import os
import threading
from abc import ABCMeta, abstractmethod
from contextlib import contextmanager
from socket import gethostname

from pathlib2 import PurePosixPath

from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.local_path import LocalPath
from framework.os_access.posix_shell import PosixShell
from framework.waiting import Wait

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


class PosixMoveLock(Lock):
    def __init__(self, posix_shell, path):
        self._shell = posix_shell  # type: PosixShell
        self._path = path  # type: PurePosixPath

    def __repr__(self):
        return '<PosixMoveLock {!r}>'.format(self._path)

    @contextmanager
    def acquired(self, timeout_sec=10):
        # Implemented as single bash script to speedup locking.
        _logger.info("Acquire lock at %r", self._path)
        comment = 'hostname: {}\npid: {}\nthread: {}'.format(gethostname(), os.getpid(), threading.current_thread())
        try:
            self._shell.run_sh_script(
                # language=Bash
                '''
                    temp_file="$(mktemp)"
                    echo "$COMMENT" > "$temp_file"
                    left_ms=$((TIMEOUT_SEC*1000))
                    while true; do
                        mv -n "$temp_file" "$LOCK_FILE"
                        if [ ! -e "$temp_file" ]; then
                            >&2 echo "Lock acquired" && exit 0
                        fi
                        if [ $left_ms -lt 0 ]; then
                            rm -v "$temp_file"
                            exit 2
                        fi
                        # Wait for random period from 0 to 500 ms.
                        ns=1$(date +%N)  # Get random, prepend with 1 to avoid interpreting as octal.
                        wait_ms=$(( (ns - 1000000000) % 500 ))  # Remove 1, get random in [0, 500).
                        left_ms=$((left_ms - wait_ms))
                        sleep $(printf "0.%03d" $wait_ms)
                    done
                    ''',
                env={'LOCK_FILE': self._path, 'TIMEOUT_SEC': timeout_sec, 'COMMENT': comment},
                timeout_sec=timeout_sec * 2)
        except exit_status_error_cls(2):
            raise AlreadyAcquired()
        try:
            yield
        finally:
            # Implemented as single bash script to speedup locking.
            _logger.info("Release lock at %r", self._path)
            try:
                self._shell.run_sh_script(
                    # language=Bash
                    '''
                        [ -e "$LOCK_FILE" ] || exit 3
                        rm "$LOCK_FILE"
                        ''',
                    env={'LOCK_FILE': self._path})
            except exit_status_error_cls(3):
                raise NotAcquired()


class LocalPosixFileLock(Lock):
    def __init__(self, path):
        self._path = path  # type: LocalPath

    def _try_lock(self, fd, timeout_sec):
        wait = Wait("lock on {} acquired".format(self._path), timeout_sec=timeout_sec)
        while True:
            try:
                _logger.info("Try lock on %r.", self._path)
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except IOError as e:
                if e.errno != errno.EAGAIN:
                    raise
                if not wait.again():
                    raise AlreadyAcquired()
                _logger.info("Sleep and again try lock on %r.", self._path)
                wait.sleep()
            else:
                _logger.info("Lock on %r acquired.", self._path)
                break

    @contextmanager
    def acquired(self, timeout_sec=10):
        with self._path.open('w') as f:
            self._try_lock(f, timeout_sec)
            yield
            # No need to unlock: file is closed.


class PosixShellFileLock(Lock):
    def __init__(self, shell, path):
        self._shell = shell  # type: PosixShell
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
