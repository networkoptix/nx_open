import logging

from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.ssh_access import SSHAccess
from framework.os_access.ssh_path import SSHPath

logger = logging.getLogger(__name__)


class MoveLockAlreadyAcquired(Exception):
    pass


class MoveLockNotAcquired(Exception):
    pass


class MoveLock(object):
    def __init__(self, ssh_access, path):
        self._ssh_access = ssh_access  # type: SSHAccess
        self._path = path  # type: SSHPath

    def __repr__(self):
        return '<{} on {}>'.format(self.__class__.__name__, self._ssh_access)

    def __enter__(self):
        # Implemented as single bash script to speedup locking.
        logger.info("Acquire lock at %r", self._path)
        try:
            self._ssh_access.run_sh_script(
                # language=Bash
                '''
                    temp_file="$(mktemp)"
                    mv -n "$temp_file" "$LOCK_FILE"
                    if [ -e "$temp_file" ]; then
                        rm -v "$temp_file"
                        exit 2
                    fi
                    ''',
                env={'LOCK_FILE': self._path})
        except exit_status_error_cls(2):
            raise MoveLockAlreadyAcquired()

    def __exit__(self, exc_type, exc_val, exc_tb):
        # Implemented as single bash script to speedup locking.
        logger.info("Release lock at %r", self._path)
        try:
            self._ssh_access.run_sh_script(
                # language=Bash
                '''
                    [ -e "$LOCK_FILE" ] || exit 3
                    rm "$LOCK_FILE"
                    ''',
                env={'LOCK_FILE': self._path})
        except exit_status_error_cls(3):
            raise MoveLockNotAcquired()
