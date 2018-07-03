import logging

from framework.os_access.path import copy_file
from framework.method_caching import cached_property
from .upstart_service import UpstartService
from .deb_installation import DebInstallation

_logger = logging.getLogger(__name__)


class DpkgInstallation(DebInstallation):
    """Install mediaserver using dpkg, control it as upstart service"""

    def __init__(self, posix_access, installer):
        dir = posix_access.Path('/opt', installer.customization.linux_subdir)
        super(DpkgInstallation, self).__init__(posix_access, installer, dir)

    @cached_property
    def service(self):
        service_name = self.installer.customization.linux_service_name
        stop_timeout_sec = 10  # 120 seconds specified in upstart conf file.
        return UpstartService(self._posix_shell, service_name, stop_timeout_sec)

    def install(self):
        if self.identity == self.installer.identity:
            _logger.info(
                'Skip installation: Existing installation identity (%s) matches installer).', self.identity)
            return

        _logger.info(
            'Perform installation: Existing installation identity (%s) does NOT match installer (%s).',
            self.identity, self.installer.identity)
        remote_path = self.os_access.Path.tmp() / self.installer.path.name
        remote_path.parent.mkdir(parents=True, exist_ok=True)
        copy_file(self.installer.path, remote_path)
        self.posix_access.ssh.run_sh_script(
            # language=Bash
            '''
                # Commands and dependencies for trusty template.
                CORE_PATTERN_FILE='/etc/sysctl.d/60-core-pattern.conf'
                echo 'kernel.core_pattern=core.%t.%p' > "$CORE_PATTERN_FILE"  # %t is timestamp, %p is pid.
                sysctl -p "$CORE_PATTERN_FILE"  # See: https://superuser.com/questions/625840
                DEBIAN_FRONTEND=noninteractive dpkg -i "$DEB"
                cp "$CONFIG" "$CONFIG_INITIAL"
                ''',
            env={
                'DEB': remote_path,
                'CONFIG': self._config,
                'CONFIG_INITIAL': self._config_initial,
                })
        assert self.is_valid()
