import abc
from collections import namedtuple

ServiceStatus = namedtuple('ServiceStatus', ['is_running', 'pid'])


class Service(object):
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def start(self):
        pass

    @abc.abstractmethod
    def stop(self):
        pass

    @abc.abstractmethod
    def status(self):
        return ServiceStatus(False, 0)

    def is_running(self):
        """Shortcut"""
        return self.status().is_running
