from abc import ABCMeta

from framework.os_access.exceptions import AlreadyDownloaded, CannotDownload
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import copy_file


class RemoteAccess(OSAccess):
    __metaclass__ = ABCMeta

    def __init__(self, host_alias, port_map):  # (ReciprocalPortMap) -> None
        self._host_alias = host_alias
        self._ports = port_map

    @property
    def alias(self):
        return self._host_alias

    @property
    def port_map(self):
        return self._ports

    def _take_local(self, local_source_path, destination_dir):
        destination = destination_dir / local_source_path.name
        if not local_source_path.exists():
            raise CannotDownload("Local file {} doesn't exist.".format(local_source_path))
        if destination.exists():
            raise AlreadyDownloaded(
                "Cannot copy {!s} to {!s}".format(local_source_path, destination_dir),
                destination)
        copy_file(local_source_path, destination)
        return destination
