import logging
from abc import ABCMeta
from io import StringIO

from framework.ini_config import IniConfig
from framework.installation.installation import Installation, OsNotSupported
from framework.os_access.posix_access import PosixAccess
from framework.os_access.posix_shell import PosixShell

# Backport provided by package `configparser` from PyPI.
# noinspection PyUnresolvedReferences,PyCompatibility
from configparser import ConfigParser

_logger = logging.getLogger(__name__)


class DebInstallation(Installation):
    """Manage installation via dpkg"""
    __metaclass__ = ABCMeta

    def __init__(self, os_access, dir, core_dumps_dirs=None):
        if not isinstance(os_access, PosixAccess):
            raise OsNotSupported(self.__class__, os_access)
        super(DebInstallation, self).__init__(
            os_access=os_access,
            dir=dir,
            binary_file=dir / 'bin' / 'mediaserver-bin',
            var_dir=dir / 'var',
            core_dumps_dirs=core_dumps_dirs or [dir / 'bin'],
            core_dump_glob='core.*',
            )
        self._posix_shell = os_access.shell  # type: PosixShell
        self._config = self.dir / 'etc' / 'mediaserver.conf'
        self._config_initial = self.dir / 'etc' / 'mediaserver.conf.initial'
        self.posix_access = os_access  # type: PosixAccess

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
                _logger.debug("Path %r exists.", path)
            else:
                _logger.debug("Path %r does not exists.", path)
                all_paths_exist = False
        return all_paths_exist

    def _can_install(self, installer):
        return installer.platform_variant == 'ubuntu' and installer.path.suffix == '.deb'

    def parse_core_dump(self, path):
        return self.os_access.parse_core_dump(path, executable_path=self.binary, lib_path=self.dir / 'lib')

    def _restore_conf(self):
        self._posix_shell.run_command(['cp', self._config_initial, self._config])

    def update_mediaserver_conf(self, new_configuration):
        old_config = self._config.read_text(encoding='ascii')
        config = ConfigParser()
        config.optionxform = lambda a: a  # Configuration is case-sensitive.
        config.readfp(StringIO(old_config))
        for name, value in new_configuration.items():
            config.set('General', name, str(value))
        f = StringIO()  # TODO: Should be text.
        config.write(f)
        _logger.debug('Write config to %s:\n%s', self._config, f.getvalue())
        self._config.write_text(f.getvalue())

    def should_reinstall(self, installer):
        if not self.is_valid():
            _logger.info('Perform installation: Existing installation is not valid/complete')
            return True
        if self.identity() == installer.identity:
            _logger.info(
                'Skip installation: Existing installation identity (%s) matches installer).', self.identity())
            return False
        else:
            _logger.info(
                'Perform installation: Existing installation identity (%s) does NOT match installer (%s).',
                self.identity(), installer.identity)
            return True

    def ini_config(self, name):
        return IniConfig(self.os_access.Path('/etc/nx_ini/{}.ini'.format(name)))

    def _find_library(self, name):
        return self.dir / 'lib' / ('lib' + name + '.so')
