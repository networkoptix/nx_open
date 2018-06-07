from abc import ABCMeta

from framework.os_access.os_access_interface import OSAccess


class RemoteAccess(OSAccess):
    __metaclass__ = ABCMeta

    def __init__(self, forwarded_ports):
        super(RemoteAccess, self).__init__()
        self._forwarded_ports = forwarded_ports

    @property
    def forwarded_ports(self):
        return self._forwarded_ports
