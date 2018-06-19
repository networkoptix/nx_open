from abc import ABCMeta, abstractproperty

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
