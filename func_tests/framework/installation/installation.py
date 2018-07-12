from abc import ABCMeta, abstractmethod, abstractproperty

from framework.installation.service import Service
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import FileSystemPath


class Installation(object):
    """Install and access installed files in uniform way"""
    __metaclass__ = ABCMeta

    def __init__(self, os_access, dir, binary_file, var_dir, core_dumps_dirs, core_dump_glob):
        self.os_access = os_access  # type: OSAccess
        self.dir = dir  # type: FileSystemPath
        self.binary = dir / binary_file  # type: FileSystemPath
        self.var = dir / var_dir  # type: FileSystemPath
        self._core_dumps_dirs = [dir / core_dumps_dir for core_dumps_dir in core_dumps_dirs]
        self._core_dump_glob = core_dump_glob  # type: str
        self.key_pair = self.var / 'ssl' / 'cert.pem'

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
    def restore_mediaserver_conf(self):
        pass

    @abstractmethod
    def update_mediaserver_conf(self, new_configuration):  # type: ({}) -> None
        pass

    def read_log(self):
        file = self.var / 'log' / 'log_file.log'
        try:
            return file.read_bytes()
        except DoesNotExist:
            return None

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
