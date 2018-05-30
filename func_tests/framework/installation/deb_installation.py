"""Manipulate directory to which server instance is installed."""

import logging
import sys
from io import BytesIO

from framework.installation.installation import Installation
from framework.installation.installer import Version, known_customizations
from framework.installation.upstart_service import UpstartService
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.path import copy_file
from framework.os_access.posix_shell import SSH

if sys.version_info[:2] == (2, 7):
    # noinspection PyCompatibility,PyUnresolvedReferences
    from ConfigParser import SafeConfigParser as ConfigParser
elif sys.version_info[:2] in {(3, 5), (3, 6)}:
    # noinspection PyCompatibility,PyUnresolvedReferences
    from configparser import ConfigParser

_logger = logging.getLogger(__name__)


class DebInstallation(Installation):
    """Either installed via dpkg or unpacked"""

    def __init__(self, ssh_access, deb):
        """Either valid or hypothetical (invalid or non-existent) installation."""
        self._ssh = ssh_access.ssh  # type: SSH
        self.installer = deb
        self.dir = ssh_access.Path('/opt', self.installer.customization.linux_subdir)
        self._bin = self.dir / 'bin'
        self.binary = self._bin / 'mediaserver-bin'
        self._config = self.dir / 'etc' / 'mediaserver.conf'
        self._config_initial = self.dir / 'etc' / 'mediaserver.conf.initial'
        self.var = self.dir / 'var'
        self._log_file = self.var / 'log' / 'log_file.log'
        self.key_pair = self.var / 'ssl' / 'cert.pem'
        self.os_access = ssh_access
        self.service = UpstartService(
            self.os_access.ssh,
            self.installer.customization.linux_service_name,
            stop_timeout_sec=120 + 10,  # 120 seconds specified in upstart conf file.
            )

    def is_valid(self):
        paths_to_check = [
            self.dir, self._bin / 'mediaserver', self.binary,
            self._config, self._config_initial]
        all_paths_exist = True
        for path in paths_to_check:
            if path.exists():
                _logger.info("Path %r exists.", path)
            else:
                _logger.warning("Path %r does not exists.", path)
                all_paths_exist = False
        return all_paths_exist

    def list_core_dumps(self):
        return self._bin.glob('core.*')

    def restore_mediaserver_conf(self):
        self._ssh.run_command(['cp', self._config_initial, self._config])

    def update_mediaserver_conf(self, new_configuration):
        old_config = self._config.read_text(encoding='ascii')
        config = ConfigParser()
        config.optionxform = str  # Configuration is case-sensitive.
        config.readfp(BytesIO(old_config.encode(encoding='ascii')))
        for name, value in new_configuration.items():
            config.set('General', name, str(value))
        f = BytesIO()  # TODO: Should be text.
        config.write(f)
        self._config.write_text(f.getvalue().decode(encoding='ascii'))

    def read_log(self):
        try:
            return self._log_file.read_bytes()
        except DoesNotExist:
            return None

    def _can_be_reused(self):
        if not self.is_valid():
            return False
        build_info_path = self.dir / 'build_info.txt'
        try:
            build_info_text = build_info_path.read_text(encoding='ascii')
        except DoesNotExist:
            return False
        build_info = dict(
            line.split('=', 1)
            for line in build_info_text.splitlines(False))
        if self.installer.version != Version(build_info['version']):
            return False
        customization, = (
            customization
            for customization in known_customizations
            if customization.customization_name == build_info['customization'])
        if self.installer.customization != customization:
            return False
        return True

    def install(self):
        if self._can_be_reused():
            return

        remote_path = self.os_access.Path.tmp() / self.installer.path.name
        remote_path.parent.mkdir(parents=True, exist_ok=True)
        copy_file(self.installer.path, remote_path)
        self.os_access.ssh.run_sh_script(
            # language=Bash
            '''
                # Commands and dependencies for trusty template.
                CORE_PATTERN_FILE='/etc/sysctl.d/60-core-pattern.conf'
                echo 'kernel.core_pattern=core.%t.%p' > "$CORE_PATTERN_FILE"  # %t is timestamp, %p is pid.
                sysctl -p "$CORE_PATTERN_FILE"  # See: https://superuser.com/questions/625840
                POINT=/mnt/trusty-packages
                mkdir -p "$POINT"
                mount -t nfs -o ro "$SHARE" "$POINT"
                DEBIAN_FRONTEND=noninteractive dpkg -i "$DEB" "$POINT"/*  # GDB (to parse core dumps) and deps.
                umount "$POINT"
                cp "$CONFIG" "$CONFIG_INITIAL"
                ''',
            env={
                'DEB': remote_path,
                'CONFIG': self._config,
                'CONFIG_INITIAL': self._config_initial,
                'SHARE': '10.0.2.107:/data/QA/func_tests/trusty-packages',
                })
        assert self.is_valid()
