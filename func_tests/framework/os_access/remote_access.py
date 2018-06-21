from abc import ABCMeta

from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import copy_file


class RemoteAccess(OSAccess):
    __metaclass__ = ABCMeta

    def __init__(self, port_map):  # (ReciprocalPortMap) -> None
        self._ports = port_map

    @property
    def port_map(self):
        return self._ports

    def _take_local(self, local_source_path, destination_dir):
        destination = destination_dir / local_source_path.name
        if not destination.exists():
            copy_file(local_source_path, destination)
        return destination
