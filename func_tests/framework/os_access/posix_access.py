import datetime
import errno
import logging
import timeit
from abc import ABCMeta
from contextlib import contextmanager

import portalocker
import pytz
import tzlocal
from netaddr import EUI
from typing import Container, Optional, Callable, ContextManager, Type

from framework.method_caching import cached_getter
from framework.networking.interface import Networking
from framework.networking.linux import LinuxNetworking
from framework.os_access import exceptions
from framework.os_access.command import DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.local_path import LocalPath
from framework.os_access.local_shell import local_shell
from framework.os_access.os_access_interface import OSAccess, OneWayPortMap, ReciprocalPortMap, Time
from framework.os_access.path import FileSystemPath
from framework.os_access.posix_shell import Shell
from framework.os_access.posix_shell_path import PosixShellPath
from framework.os_access.ssh_shell import SSH
from framework.os_access.ssh_traffic_capture import SSHTrafficCapture
from framework.os_access.traffic_capture import TrafficCapture
from framework.utils import RunningTime

_logger = logging.getLogger(__name__)

MAKE_CORE_DUMP_TIMEOUT_SEC = 60 * 5


class _LocalTime(Time):
    def __init__(self):
        self._tz = tzlocal.get_localzone()

    def get(self):  # type: () -> RunningTime
        now = datetime.datetime.now(tz=self._tz)
        return RunningTime(now)

    def set(self, aware_datetime):  # type: (datetime) -> RunningTime
        raise NotImplementedError("Setting time is prohibited on local machine")


class _ReadOnlyPosixTime(Time):
    def __init__(self, shell):  # type: (Shell) -> None
        self._shell = shell

    def get(self):
        started_at = timeit.default_timer()
        timestamp_output = self._shell.command(['date', '+%s']).run(timeout_sec=2)
        timestamp = int(timestamp_output.decode('ascii').rstrip())
        delay_sec = timeit.default_timer() - started_at
        timezone_output = self._shell.command(['cat', '/etc/timezone']).run(timeout_sec=2)
        timezone_name = timezone_output.decode('ascii').rstrip()
        timezone = pytz.timezone(timezone_name)
        local_time = datetime.datetime.fromtimestamp(timestamp, tz=timezone)
        return RunningTime(local_time, datetime.timedelta(seconds=delay_sec))

    def set(self, aware_datetime):
        raise NotImplementedError("Setting time is prohibited on {!r}".format(self._shell))


class _PosixTime(_ReadOnlyPosixTime):
    def set(self, new_time):
        started_at = datetime.datetime.now(pytz.utc)
        self._shell.command(['date', '--set', new_time.isoformat()]).run()
        return RunningTime(new_time, datetime.datetime.now(pytz.utc) - started_at)


local_time = _LocalTime()


class PosixAccess(OSAccess):
    __metaclass__ = ABCMeta

    def __init__(
            self,
            alias,  # type: str
            port_map,  # type: ReciprocalPortMap
            shell,  # type: Shell
            time,  # type: Time
            traffic_capture,  # type: Optional[TrafficCapture]
            lock_acquired,  # type: Optional[Callable[[FileSystemPath, ...], ContextManager[None]]]
            path_cls,  # type: Type[FileSystemPath]
            networking,  # type: Optional[Networking]
            ):
        super(PosixAccess, self).__init__(
            alias,
            port_map,
            networking, time, traffic_capture, lock_acquired,
            path_cls)
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
        path_cls = PosixShellPath.specific_cls(ssh)
        traffic_capture = SSHTrafficCapture(ssh, path_cls.tmp() / 'traffic_capture')
        return cls(
            vm_alias,
            port_map,
            ssh, _PosixTime(ssh), traffic_capture, ssh.lock_acquired, path_cls,
            LinuxNetworking(ssh, macs))

    @classmethod
    def to_physical_machine(cls, alias, address, ssh_user_name, ssh_private_key):
        # type: (str, ReciprocalPortMap, str, str) -> PosixAccess
        port_map = ReciprocalPortMap(OneWayPortMap.direct(address), OneWayPortMap.empty())
        ssh = cls._make_ssh(port_map, ssh_user_name, ssh_private_key)
        return cls(
            alias,
            port_map,
            ssh, _ReadOnlyPosixTime(ssh), None, ssh.lock_acquired, PosixShellPath.specific_cls(ssh),
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
            command.run(timeout_sec=MAKE_CORE_DUMP_TIMEOUT_SEC)
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
        mount_point = self.path_cls('/mnt') / name
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

    def free_disk_space_bytes(self):
        command = self.shell.command([
            'df',
            '--output=target,avail',  # Only mount point (target) and free (available) space.
            '--block-size=1',  # By default it's 1024 and all values are in kilobytes.
            ])
        output = command.run()
        for line in output.splitlines()[1:]:  # Mind header.
            mount_point, free_space_raw = line.split()
            if mount_point == '/':
                return int(free_space_raw)
        raise RuntimeError("Cannot find mount point / in output:\n{}".format(output))

    def consume_disk_space(self, should_leave_bytes):
        to_consume_bytes = self.free_disk_space_bytes() - should_leave_bytes
        holder_path = self._disk_space_holder()
        self.shell.command(['fallocate', '-l', to_consume_bytes, holder_path]).run()

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
