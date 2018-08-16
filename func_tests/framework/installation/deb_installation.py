import logging
import sys
from abc import ABCMeta
from io import BytesIO

from framework.ini_config import IniConfig
from framework.installation.installation import Installation
from framework.installation.installer import InstallIdentity
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.posix_access import PosixAccess
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
    __metaclass__ = ABCMeta

    _NOT_SET = object()

    def __init__(self, posix_access, dir, core_dumps_dirs=None):
        super(DebInstallation, self).__init__(
            os_access=posix_access,
            dir=dir,
            binary_file=dir / 'bin' / 'mediaserver-bin',
            var_dir=dir / 'var',
            core_dumps_dirs=core_dumps_dirs or [dir / 'bin'],
            core_dump_glob='core.*',
            )
        self._posix_shell = posix_access.shell  # type: PosixShell
        self._config = self.dir / 'etc' / 'mediaserver.conf'
        self._config_initial = self.dir / 'etc' / 'mediaserver.conf.initial'
        self.posix_access = posix_access  # type: PosixAccess
        self._identity = self._NOT_SET

    @property
    def paths_to_validate(self):
        return [
            self.dir,
            self.binary,
            self._config,
            self._config_initial,
            ]

    def is_valid(self):
        all_paths_exist = True
        for path in self.paths_to_validate:
            if path.exists():
                _logger.info("Path %r exists.", path)
            else:
                _logger.warning("Path %r does not exists.", path)
                all_paths_exist = False
        return all_paths_exist

    def parse_core_dump(self, path):
        return self.os_access.parse_core_dump(path, executable_path=self.binary, lib_path=self.dir / 'lib')

    def _restore_conf(self):
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
        _logger.debug('Write config to %s:\n%s', self._config, f.getvalue())
        self._config.write_text(f.getvalue().decode(encoding='ascii'))

    # returns None if server is not installed (yet)
    # cached_property does not fit because we need to invalidate it after .install()
    @property
    def identity(self):
        if self._identity is self._NOT_SET:
            self._identity = self._discover_identity()
        return self._identity

    def _discover_identity(self):
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
        if not self.is_valid():
            _logger.info('Perform installation: Existing installation is not valid/complete')
            return True
        if self.identity == installer.identity:
            _logger.info(
                'Skip installation: Existing installation identity (%s) matches installer).', self.identity)
            return False
        else:
            _logger.info(
                'Perform installation: Existing installation identity (%s) does NOT match installer (%s).',
                self.identity, installer.identity)
            return True

    def ini_config(self, name):
        return IniConfig(self.os_access.Path('/etc/nx_ini/{}.ini'.format(name)))

    def _find_library(self, name):
        return self.dir / 'lib' / ('lib' + name + '.so')
