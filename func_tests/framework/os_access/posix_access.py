from abc import ABCMeta, abstractproperty

from framework.method_caching import cached_getter
from framework.os_access.command import DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.exceptions import (
    AlreadyDownloaded,
    CannotDownload,
    NonZeroExitStatus,
    exit_status_error_cls,
    )
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.posix_shell import PosixShell

MAKE_CORE_DUMP_TIMEOUT_SEC = 60 * 5


class CoreDumpError(Exception):

    def __init__(self, error):
        super(CoreDumpError, self).__init__('Failed to make core dump: %s' % error)
        self.error = error


class PosixAccess(OSAccess):
    __metaclass__ = ABCMeta

    @cached_getter
    def env_vars(self):
        output = self.run_command(['env'])
        result = {}
        for line in output.rstrip().splitlines():
            name, value = line.split('=', 1)
            result[name] = value
        return result

    @abstractproperty
    def shell(self):
        return PosixShell()

    def run_command(self, command, input=None, logger=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC):
        return self.shell.run_command(command, input=input, logger=logger, timeout_sec=timeout_sec)

    def make_core_dump(self, pid):
        try:
            self.shell.run_sh_script(
                'gcore -o /proc/$PID/cwd/core.$(date +%s) $PID',
                env={'PID': pid},
                timeout_sec=MAKE_CORE_DUMP_TIMEOUT_SEC)
        except exit_status_error_cls(1) as e:
            lines = e.stderr.splitlines()
            if "You can't do that without a process to debug." in lines:
                # first stderr line from gcore contains actual error, failure reason such as:
                # "Unable to attach: program terminated with signal SIGSEGV, Segmentation fault."
                # or:
                # "warning: process 7570 is a zombie - the process has already terminated"
                raise CoreDumpError(lines[0])
            else:
                raise

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
            raise AlreadyDownloaded(
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
        except NonZeroExitStatus as e:
            raise CannotDownload(e.stderr)
        return destination

    def _download_by_smb(self, source_hostname, source_path, destination_dir, timeout_sec):
        url = 'smb://{!s}/{!s}'.format(source_hostname, '/'.join(source_path.parts))
        destination = destination_dir / source_path.name
        if destination.exists():
            raise AlreadyDownloaded(
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
        except NonZeroExitStatus as e:
            raise CannotDownload(e.stderr)
        return destination
