from abc import ABCMeta

from framework.os_access.os_access_interface import OSAccess


class RemoteAccess(OSAccess):
    __metaclass__ = ABCMeta

    def __init__(self, port_map):  # (ReciprocalPortMap) -> None
        self._ports = port_map

    @property
    def port_map(self):
        return self._ports
