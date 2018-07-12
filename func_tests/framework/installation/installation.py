import logging
from abc import ABCMeta, abstractmethod, abstractproperty

from framework.installation.service import Service
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import FileSystemPath

_logger = logging.getLogger(__name__)


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

    @abstractproperty
    def identity(self):
        pass

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
        return self._var.joinpath('log').glob('log_file*.log')

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
        _logger.info("Remove old core dumps.")
        for core_dump_path in self.list_core_dumps():
            core_dump_path.unlink()
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
