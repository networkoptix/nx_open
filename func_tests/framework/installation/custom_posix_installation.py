from .deb_installation import DebInstallation
from .upstart_service import LinuxAdHocService
from ..method_caching import cached_property


class CustomPosixInstallation(DebInstallation):
    """Mediaserver installation intended to be unpacked from a deb file to a directory,
       and run in user mode controlled by custom bash scripts"""

    @cached_property
    def service(self):
        return LinuxAdHocService(self._posix_shell, self.dir)
