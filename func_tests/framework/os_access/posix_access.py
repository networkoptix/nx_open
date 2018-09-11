import datetime
import errno
import logging
from abc import ABCMeta
from contextlib import contextmanager

import portalocker
import tzlocal
from netaddr import EUI
from typing import Container

from framework.method_caching import cached_getter
from framework.networking.linux import LinuxNetworking
from framework.os_access import exceptions
from framework.os_access.command import DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.local_path import LocalPath
from framework.os_access.local_shell import local_shell
from framework.os_access.os_access_interface import OSAccess, ReciprocalPortMap, OneWayPortMap
from framework.os_access.posix_shell import ReadOnlyTime, Time
from framework.os_access.posix_shell_path import PosixShellPath
from framework.os_access.ssh_shell import SSH
from framework.os_access.ssh_traffic_capture import SSHTrafficCapture

_logger = logging.getLogger(__name__)

MAKE_CORE_DUMP_TIMEOUT_SEC = 60 * 5


class _LocalTime(object):
    def __init__(self):
        self._tz = tzlocal.get_localzone()

    def get(self):
        now = datetime.datetime.now(tz=self._tz)
        return now


local_time = _LocalTime()


class PosixAccess(OSAccess):
    __metaclass__ = ABCMeta

    def __init__(
            self,
            alias,
            port_map,
            shell, time, traffic_capture, lock_acquired,
            Path, networking):
        super(PosixAccess, self).__init__(
            alias,
            port_map,
            networking, time, traffic_capture, lock_acquired,
            Path)
        self.shell = shell

    @staticmethod
    def _make_ssh(port_map, user_name, private_key):
        address = port_map.remote.address
        port = port_map.remote.tcp(22)
        ssh = SSH(address, port, user_name, private_key)
        return ssh

    @classmethod
    def to_vm(cls, vm_alias, port_map, macs, ssh_user_name, ssh_private_key):
        # type: (str, ReciprocalPortMap, str, str, Container[EUI]) -> PosixAccess
        ssh = cls._make_ssh(port_map, ssh_user_name, ssh_private_key)
        Path = PosixShellPath.specific_cls(ssh)
        traffic_capture = SSHTrafficCapture(ssh, Path.tmp() / 'traffic_capture')
        return cls(
            vm_alias,
            port_map,
            ssh, Time(ssh), traffic_capture, ssh.lock_acquired, Path,
            LinuxNetworking(ssh, macs))

    @classmethod
    def to_physical_machine(cls, alias, address, ssh_user_name, ssh_private_key):
        # type: (str, ReciprocalPortMap, str, str) -> PosixAccess
        port_map = ReciprocalPortMap(OneWayPortMap.direct(address), OneWayPortMap.empty())
        ssh = cls._make_ssh(port_map, ssh_user_name, ssh_private_key)
        return cls(
            alias,
            port_map,
            ssh, ReadOnlyTime(ssh), None, ssh.lock_acquired, PosixShellPath.specific_cls(ssh),
            None)

    def is_accessible(self):
        return self.shell.is_working()

    @cached_getter
    def env_vars(self):
        output = self.run_command(['env'])
        result = {}
        for line in output.decode('ascii').rstrip().splitlines():
            name, value = line.split(u'=', 1)
            result[name] = value
        return result

    def run_command(self, command, input=None, logger=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC):
        return self.shell.run_command(command, input=input, logger=logger, timeout_sec=timeout_sec)

    def make_core_dump(self, pid):
        _logger.info('Making core dump for pid %d', pid)
        try:
            command = self.shell.sh_script(
                'gcore -o /proc/$PID/cwd/core.$(date +%s) $PID',
                env={'PID': pid},
                set_eux=False,
                )
            command.check_output(timeout_sec=MAKE_CORE_DUMP_TIMEOUT_SEC)
        except exceptions.exit_status_error_cls(1) as e:
            if "You can't do that without a process to debug." not in e.stderr:
                raise
            # first stderr line from gcore contains actual error, failure reason such as:
            # "Unable to attach: program terminated with signal SIGSEGV, Segmentation fault."
            # or:
            # "warning: process 7570 is a zombie - the process has already terminated"
            lines = e.stderr.splitlines()
            raise exceptions.CoreDumpError(lines[0])

    def parse_core_dump(self, path, executable_path=None, lib_path=None, timeout_sec=600):
        output = self.run_command([
            'gdb',
            '--quiet',
            '--core', path,
            '--exec', executable_path,
            '--eval-command', 'set solib-search-path {}'.format(lib_path),
            '--eval-command', 'set print static-members off',
            '--eval-command', 'thread apply all backtrace',
            '--eval-command', 'quit',
            ],
            timeout_sec=timeout_sec)
        return output.decode('ascii')

    def make_fake_disk(self, name, size_bytes):
        mount_point = self.Path('/mnt') / name
        image_path = mount_point.with_suffix('.image')
        self.shell.run_sh_script(
            # language=Bash
            '''
                ! mountpoint "$MOUNT_POINT" || umount "$MOUNT_POINT"
                rm -fv "$IMAGE"
                fallocate --length $SIZE "$IMAGE"
                mke2fs -F "$IMAGE"  # Make default filesystem for OS.
                mkdir -p "$MOUNT_POINT"
                mount "$IMAGE" "$MOUNT_POINT"
                ''',
            env={'MOUNT_POINT': mount_point, 'IMAGE': image_path, 'SIZE': size_bytes})
        return mount_point

    def _download_by_http(self, source_url, destination_dir, timeout_sec):
        _, file_name = source_url.rsplit('/', 1)
        destination = destination_dir / file_name
        if destination.exists():
            raise exceptions.AlreadyExists(
                "Cannot download {!s} to {!s}".format(source_url, destination_dir),
                destination)
        try:
            self.shell.run_command(
                [
                    'wget',
                    '--no-clobber', source_url,  # Don't overwrite file.
                    '--output-document', destination,
                    ],
                timeout_sec=timeout_sec,
                )
        except exceptions.NonZeroExitStatus as e:
            raise exceptions.CannotDownload(e.stderr)
        return destination

    def _download_by_smb(self, source_hostname, source_path, destination_dir, timeout_sec):
        url = 'smb://{!s}/{!s}'.format(source_hostname, '/'.join(source_path.parts))
        destination = destination_dir / source_path.name
        if destination.exists():
            raise exceptions.AlreadyExists(
                "Cannot download file {!s} from {!s} to {!s}".format(
                    source_path, source_hostname, destination_dir),
                destination)
        # TODO: Decide on authentication based on username and password from URL.
        try:
            self.shell.run_command(
                [
                    'smbget',
                    '--quiet',  # Usual progress output has outrageous rate.
                    '--guest',  # Force guest authentication.
                    '--outputfile', destination,
                    url,
                    ],
                timeout_sec=timeout_sec,
                )
        except exceptions.NonZeroExitStatus as e:
            raise exceptions.CannotDownload(e.stderr)
        return destination


@contextmanager
def portalocker_lock_acquired(path, timeout_sec=10):
    try:
        with portalocker.utils.Lock(str(path), timeout=timeout_sec):
            yield
    except portalocker.exceptions.LockException as e:
        while not isinstance(e, IOError):
            e, = e.args
        if e.errno != errno.EAGAIN:
            raise e
        raise exceptions.AlreadyAcquired()


local_access = PosixAccess(
    'localhost',
    ReciprocalPortMap(OneWayPortMap.local(), OneWayPortMap.local()),
    local_shell, local_time, None, portalocker_lock_acquired, LocalPath,
    None)
