import logging
import re
from abc import ABCMeta, abstractmethod, abstractproperty

from framework.installation.installer import InstallIdentity
from framework.installation.service import Service
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import FileSystemPath

_logger = logging.getLogger(__name__)


class NotInstalled(Exception):
    pass


class Installation(object):
    """Install and access installed files in uniform way"""
    __metaclass__ = ABCMeta

    def __init__(self, os_access, dir, binary_file, var_dir, core_dumps_dirs, core_dump_glob):
        self.os_access = os_access  # type: OSAccess
        self.dir = dir  # type: FileSystemPath
        self.binary = dir / binary_file  # type: FileSystemPath
        self._var = dir / var_dir  # type: FileSystemPath
        self._core_dumps_dirs = [dir / core_dumps_dir for core_dumps_dir in core_dumps_dirs]
        self._core_dump_glob = core_dump_glob  # type: str

    def _build_info(self):
        path = self.dir / 'build_info.txt'
        try:
            text = path.read_text(encoding='ascii')
        except DoesNotExist as e:
            raise NotInstalled(e)
        parsed = dict(line.split('=', 1) for line in text.splitlines(False))
        return parsed

    def identity(self):
        return InstallIdentity.from_build_info(self._build_info())

    @abstractmethod
    def install(self, installer):
        pass

    @abstractmethod
    def is_valid(self):
        return False

    @abstractproperty
    def service(self):
        return Service()

    @abstractmethod
    def _restore_conf(self):
        pass

    @abstractmethod
    def update_mediaserver_conf(self, new_configuration):  # type: ({}) -> None
        pass

    def list_log_files(self):
        logs_dir = self._var.joinpath('log')
        if logs_dir.exists():
            return logs_dir.glob('log_file*.log')
        else:
            return []

    def list_core_dumps(self):
        return [
            path
            for dir in self._core_dumps_dirs
            for path in dir.glob(self._core_dump_glob)
            ]

    @abstractmethod
    def parse_core_dump(self, path):
        """Parse MediaServer process dump"""
        return u''

    def cleanup(self, new_key_pair):
        self.cleanup_core_dumps()
        try:
            _logger.info("Remove var directory %s.", self._var)
            self._var.rmtree()
        except DoesNotExist:
            pass
        key_pair_file = self._var / 'ssl' / 'cert.pem'
        _logger.info("Put key pair to %s.", key_pair_file)
        key_pair_file.parent.mkdir(parents=True, exist_ok=True)
        key_pair_file.write_text(new_key_pair)
        _logger.info("Update conf file.")
        self._restore_conf()
        self.update_mediaserver_conf({
            'logLevel': 'DEBUG2',
            'tranLogLevel': 'DEBUG2',
            'checkForUpdateUrl': 'http://127.0.0.1:8080',  # TODO: Use fake server responding with small updates.
            })

    def cleanup_core_dumps(self):
        _logger.info("Remove old core dumps.")
        for core_dump_path in self.list_core_dumps():
            core_dump_path.unlink()

    def specific_features(self):
        path = self.dir / 'specific_features.txt'
        try:
            return path.read_text(encoding='ascii').splitlines()
        except DoesNotExist:
            return []

    @abstractmethod
    def ini_config(self, name):
        pass

    @abstractmethod
    def _find_library(self, name):
        """Similar to `ctypes.util.find_library()`."""
        return FileSystemPath()

    def _cloud_host_lib(self):
        version = self.identity().version
        name = 'nx_network' if version >= (4, 0) else 'common'
        return self._find_library(name)

    _cloud_host_regex = re.compile(br'this_is_cloud_host_name (?P<value>.+?)(?P<padding>\0*)\0')

    def set_cloud_host(self, new_host):
        if self.service.status().is_running:
            raise RuntimeError("Mediaserver must be stopped to patch cloud host.")
        return self._cloud_host_lib().patch_string(self._cloud_host_regex, new_host.encode('ascii'), '.cloud_host_offset')

    def reset_default_cloud_host(self):
        cloud_host = self._build_info()['cloudHost']
        return self.set_cloud_host(cloud_host)
