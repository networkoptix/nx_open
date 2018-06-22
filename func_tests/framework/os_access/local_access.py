import datetime
import errno
import os

import tzlocal

from framework.networking.prohibited import ProhibitedNetworking
from framework.os_access.exceptions import AlreadyDownloaded
from framework.os_access.local_path import LocalPath
from framework.os_access.os_access_interface import OneWayPortMap, ReciprocalPortMap
from framework.os_access.posix_access import PosixAccess
from framework.os_access.posix_shell import local_shell


class LocalAccess(PosixAccess):
    def is_accessible(self):
        return True

    @property
    def shell(self):
        return local_shell

    @property
    def Path(self):
        return LocalPath

    @property
    def networking(self):
        return ProhibitedNetworking()

    @property
    def port_map(self):
        return ReciprocalPortMap(OneWayPortMap.local(), OneWayPortMap.local())

    def get_time(self):
        local_timezone = tzlocal.get_localzone()
        local_datetime = datetime.datetime.now(tz=local_timezone)
        return local_datetime

    def set_time(self, new_time):
        raise NotImplementedError("Changing local time is prohibited")

    def _take_local(self, local_source_path, destination_dir):
        destination = destination_dir / local_source_path.name
        try:
            os.symlink(str(local_source_path), str(destination))
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise
            raise AlreadyDownloaded(
                "Creating symlink {!s} pointing to {!s}".format(destination, local_source_path),
                destination)
        return destination


local_access = LocalAccess()
