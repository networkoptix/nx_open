import logging
import sys
from io import BytesIO

from framework.installation.installation import Installation
from framework.installation.installer import InstallIdentity
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.posix_shell import PosixShell

if sys.version_info[:2] == (2, 7):
    # noinspection PyCompatibility,PyUnresolvedReferences
    from ConfigParser import SafeConfigParser as ConfigParser
elif sys.version_info[:2] in {(3, 5), (3, 6)}:
    # noinspection PyCompatibility,PyUnresolvedReferences
    from configparser import ConfigParser

_logger = logging.getLogger(__name__)


class DebInstallation(Installation):
    """Manage installation via dpkg"""

    _NOT_SET = object()

    def __init__(self, posix_access, dir):
        self._posix_shell = posix_access.shell  # type: PosixShell
        self.dir = dir
        self._bin = self.dir / 'bin'
        self.binary = self._bin / 'mediaserver-bin'
        self._config = self.dir / 'etc' / 'mediaserver.conf'
        self._config_initial = self.dir / 'etc' / 'mediaserver.conf.initial'
        self.var = self.dir / 'var'
        self._log_file = self.var / 'log' / 'log_file.log'
        self.key_pair = self.var / 'ssl' / 'cert.pem'
        self.posix_access = posix_access
        self._identity = self._NOT_SET

    @property
    def os_access(self):
        return self.posix_access

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

    def parse_core_dump(self, path):
        return self.os_access.parse_core_dump(path, self.binary, self.dir / 'lib')

    def restore_mediaserver_conf(self):
        self._posix_shell.run_command(['cp', self._config_initial, self._config])

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

    # returns None if server is not installed (yet)
    # cached_property does not fit because we need to invalidate it after .install()
    @property
    def identity(self):
        if self._identity is self._NOT_SET:
            self._identity = self._discover_identity()
        return self._identity

    def _discover_identity(self):
        if not self.is_valid():
            return None
        build_info_path = self.dir / 'build_info.txt'
        try:
            build_info_text = build_info_path.read_text(encoding='ascii')
        except DoesNotExist:
            return None
        build_info = dict(
            line.split('=', 1)
            for line in build_info_text.splitlines(False))
        return InstallIdentity.from_build_info(build_info)

    def should_reinstall(self, installer):
        if self.identity == installer.identity:
            _logger.info(
                'Skip installation: Existing installation identity (%s) matches installer).', self.identity)
            return False
        else:
            _logger.info(
                'Perform installation: Existing installation identity (%s) does NOT match installer (%s).',
                self.identity, installer.identity)
            return True
