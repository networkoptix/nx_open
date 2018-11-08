import datetime
import errno
import logging
import shlex
import timeit
from abc import ABCMeta
from contextlib import contextmanager

import portalocker
import pytz
import tzlocal
from netaddr import EUI
from typing import Any, Container, Optional, Callable, Type

from framework.method_caching import cached_getter
from framework.networking.interface import Networking
from framework.networking.linux import LinuxNetworking
from framework.os_access import exceptions
from framework.os_access.command import DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.local_path import LocalPath
from framework.os_access.local_shell import local_shell
from framework.os_access.os_access_interface import BaseFakeDisk, OSAccess, OneWayPortMap, ReciprocalPortMap, Time
from framework.os_access.path import FileSystemPath
from framework.os_access.posix_shell import Shell
from framework.os_access.sftp_path import SftpPath
from framework.os_access.ssh_shell import SSH
from framework.os_access.ssh_traffic_capture import SSHTrafficCapture
from framework.os_access.traffic_capture import TrafficCapture
from framework.utils import RunningTime

_logger = logging.getLogger(__name__)

MAKE_CORE_DUMP_TIMEOUT_SEC = 60 * 5


class _LocalTime(Time):
    def get_tz(self):
        return tzlocal.get_localzone()

    def get(self):  # type: () -> RunningTime
        now = datetime.datetime.now(tz=self.get_tz())
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
        local_time = datetime.datetime.fromtimestamp(timestamp, tz=self.get_tz())
        return RunningTime(local_time, datetime.timedelta(seconds=delay_sec))

    def get_tz(self):
        timezone_output = self._shell.command(['cat', '/etc/timezone']).run(timeout_sec=2)
        timezone_name = timezone_output.decode('ascii').rstrip()
        timezone = pytz.timezone(timezone_name)
        return timezone

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
            lock_acquired,  # type: Optional[Callable[[FileSystemPath, ...], Any]]
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
        path_cls = SftpPath.specific_cls(ssh)
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
            ssh, _ReadOnlyPosixTime(ssh), None, ssh.lock_acquired, SftpPath.specific_cls(ssh),
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

    def fake_disk(self):
        return _FakeDisk(self)

    def _fs_root(self):
        return self.path_cls('/')

    def _free_disk_space_bytes_on_all(self):
        command = self.shell.command([
            'df',
            '--output=target,avail',  # Only mount point (target) and free (available) space.
            '--block-size=1',  # By default it's 1024 and all values are in kilobytes.
            ])
        output = command.run()
        result = {}
        for line in output.splitlines()[1:]:  # Mind header.
            target, avail = line.split()
            result[self.path_cls(target)] = int(avail)
        return result

    def _hold_disk_space(self, to_consume_bytes):
        holder_path = self._disk_space_holder()
        self.shell.command(['fallocate', '-l', to_consume_bytes, holder_path]).run()

    def _download_by_http(self, source_url, destination_dir, timeout_sec):
        _, file_name = source_url.rsplit('/', 1)
        destination = destination_dir / file_name
        if destination.exists():
            return destination
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
            return destination
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

    def _files_md5(self, paths):
        _logger.debug("Get MD5 of %d paths:\n%s", len(paths), '\n'.join(str(p) for p in paths))
        if not paths:
            return
        command = self.shell.command(['md5sum', '--binary'] + paths)
        with command.running() as run:
            stdout, stderr = run.communicate(timeout_sec=300)
        for line in stdout.decode().splitlines():
            if not line:  # Last line is empty.
                continue
            assert line[32] == ' '
            assert line[33] == '*'  # `*` appears if file is treated as binary.
            yield self.path_cls(line[34:]), line[:32]
        for line in stderr.decode().splitlines():
            if not line or line.startswith('+'):  # Last empty line and tracing from set `-x`.
                continue
            if not line.endswith(': Is a directory'):
                raise RuntimeError('Cannot calculate MD5 on {}:\n{}'.format(self, stderr))

    def file_md5(self, file_path):
        (path, digest), = self._files_md5([file_path])
        return digest

    def tree_md5(self, tree_path):
        _logger.debug("Get MD5 of each file under: %s", tree_path)
        paths = list(tree_path.glob('**/*'))
        result = {}
        for path, digest in self._files_md5(paths):
            parts = path.relative_to(tree_path).parts
            result[parts] = digest
        return result


class _FakeDisk(BaseFakeDisk):
    def __init__(self, posix_access):  # type: (PosixAccess) -> ...
        super(_FakeDisk, self).__init__(posix_access.path_cls('/mnt/disk'))
        self._image_path = self.path.with_suffix('.img')
        self._shell = posix_access.shell

    def remove(self):
        # At least, in Ubuntu 14.04 trusty, multiple `/dev/loop*` devices may be mounted to single
        # mount point. Files that are behind `/dev/loop*` may be deleted. To dismount such mounts,
        # `umount -f /mnt/point` must be called unless it exists with error code 1 and says
        # `umount2: Invalid argument` and `umount: /mnt/disk: not mounted`.
        while True:
            try:
                self._shell.run_command(['umount', '-f', self.path])
            except exceptions.exit_status_error_cls(1) as e:
                if not e.stderr.decode().rstrip().endswith(': not mounted'):
                    raise
                break
        # File should be deleted too. Otherwise, it may contain data from previous test.
        try:
            self._image_path.unlink()
        except exceptions.DoesNotExist:
            pass

    def mount(self, size_bytes):
        disk_bytes = size_bytes + 26 * 1024 * 1024  # Taken by filesystem.
        self._shell.run_sh_script(
            # language=Bash
            '''
                fallocate --length $SIZE "$IMAGE"
                mke2fs -F "$IMAGE"  # Make default filesystem for OS.
                mkdir -p "$MOUNT_POINT"
                mount "$IMAGE" "$MOUNT_POINT"
                ''',
            env={'MOUNT_POINT': self.path, 'IMAGE': self._image_path, 'SIZE': disk_bytes})


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
