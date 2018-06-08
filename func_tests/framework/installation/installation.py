from abc import ABCMeta, abstractmethod

from framework.os_access.path import FileSystemPath


class Installation(object):
    """Install and access installed files in uniform way"""
    __metaclass__ = ABCMeta

    @abstractmethod
    def install(self):
        pass

    @abstractmethod
    def is_valid(self):
        return False

    @abstractmethod
    def list_core_dumps(self):
        return [FileSystemPath()]

    @abstractmethod
    def restore_mediaserver_conf(self):
        pass

    @abstractmethod
    def update_mediaserver_conf(self, new_configuration):  # type: ({}) -> None
        pass

    @abstractmethod
    def read_log(self):
        return b'log file contents'
