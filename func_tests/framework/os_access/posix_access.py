from abc import ABCMeta, abstractproperty

from framework.move_lock import MoveLock
from framework.os_access.exceptions import AlreadyDownloaded, CannotDownload, NonZeroExitStatus
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.posix_shell import PosixShell


class PosixAccess(OSAccess):
    __metaclass__ = ABCMeta

    @abstractproperty
    def shell(self):
        return PosixShell()

    def run_command(self, command, input=None):
        return self.shell.run_command(command, input=input)

    def make_core_dump(self, pid):
        self.shell.run_sh_script('gcore -o /proc/$PID/cwd/core.$(date +%s) $PID', env={'PID': pid})

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

    def _download_by_http(self, source_url, destination_dir):
        _, file_name = source_url.rsplit('/', 1)
        destination = destination_dir / file_name
        if destination.exists():
            raise AlreadyDownloaded(
                "Cannot download {!s} to {!s}".format(source_url, destination_dir),
                destination)
        try:
            self.shell.run_command([
                'wget',
                '--no-clobber', source_url,  # Don't overwrite file.
                '--output-document', destination,
                ])
        except NonZeroExitStatus as e:
            raise CannotDownload(e.stderr)
        return destination

    def _download_by_smb(self, source_hostname, source_path, destination_dir):
        url = 'smb://{!s}/{!s}'.format(source_hostname, '/'.join(source_path.parts))
        destination = destination_dir / source_path.name
        if destination.exists():
            raise AlreadyDownloaded(
                "Cannot download file {!s} from {!s} to {!s}".format(
                    source_path, source_hostname, destination_dir),
                destination)
        # TODO: Decide on authentication based on username and password from URL.
        try:
            self.shell.run_command([
                'smbget',
                '--quiet',  # Usual progress output has outrageous rate.
                '--guest',  # Force guest authentication.
                '--outputfile', destination,
                url,
                ])
        except NonZeroExitStatus as e:
            raise CannotDownload(e.stderr)
        return destination

    def lock(self, path, try_lock_timeout_sec=10):
        return MoveLock(self.shell, path)
