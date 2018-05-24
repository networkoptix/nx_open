from abc import ABCMeta, abstractmethod

from framework.os_access.path import FileSystemPath


class Installation(object):
    __metaclass__ = ABCMeta

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

    @abstractmethod
    def patch_binary_set_cloud_host(self, new_host):  # type: (str) -> None
        pass
