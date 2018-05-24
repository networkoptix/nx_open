from abc import ABCMeta, abstractmethod, abstractproperty
from datetime import datetime

from netaddr import IPAddress

from framework.networking.interface import Networking
from framework.os_access.path import FileSystemPath
from framework.utils import RunningTime


class OSAccess(object):
    __metaclass__ = ABCMeta

    @abstractmethod
    def run_command(self, command, input=None):  # type: (list, bytes) -> bytes
        """For applications with cross-platform CLI"""
        return b'stdout'

    @abstractmethod
    def is_accessible(self):
        return False

    # noinspection PyPep8Naming
    @abstractproperty
    def Path(self):
        return FileSystemPath

    @abstractproperty
    def networking(self):
        return Networking()

    @abstractproperty
    def forwarded_ports(self):
        return {('tcp', 22): (IPAddress('127.0.0.1'), 60022)}

    @abstractmethod
    def get_time(self):
        pass

    @abstractmethod
    def set_time(self, new_time):  # type: (datetime.datetime) -> RunningTime
        return RunningTime(datetime.now())
