import logging

from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.ssh_shell import SSH
from framework.os_access.ssh_path import SSHPath

_logger = logging.getLogger(__name__)


class MoveLockAlreadyAcquired(Exception):
    pass


class MoveLockNotAcquired(Exception):
    pass


class MoveLock(object):
    # TODO: Replace with lockfile (at least, it's in a pip/vendor)
    def __init__(self, ssh, path, timeout_sec=60):
        self._ssh = ssh  # type: SSH
        self._path = path  # type: SSHPath
        self._timeout_sec = timeout_sec

    def __repr__(self):
        return '<MoveLock {!r}>'.format(self._path)

    def __enter__(self):
        # Implemented as single bash script to speedup locking.
        _logger.info("Acquire lock at %r", self._path)
        try:
            self._ssh.run_sh_script(
                # language=Bash
                '''
                    temp_file="$(mktemp)"
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
                env={'LOCK_FILE': self._path, 'TIMEOUT_SEC': self._timeout_sec},
                timeout_sec=self._timeout_sec * 2)
        except exit_status_error_cls(2):
            raise MoveLockAlreadyAcquired()

    def __exit__(self, exc_type, exc_val, exc_tb):
        # Implemented as single bash script to speedup locking.
        _logger.info("Release lock at %r", self._path)
        try:
            self._ssh.run_sh_script(
                # language=Bash
                '''
                    [ -e "$LOCK_FILE" ] || exit 3
                    rm "$LOCK_FILE"
                    ''',
                env={'LOCK_FILE': self._path})
        except exit_status_error_cls(3):
            raise MoveLockNotAcquired()
