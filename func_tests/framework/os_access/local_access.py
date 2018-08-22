import datetime
import errno
import logging
import os

import tzlocal

from framework.lock import PortalockerLock
from framework.networking.prohibited import ProhibitedNetworking
from framework.os_access.exceptions import AlreadyDownloaded, CannotDownload
from framework.os_access.local_path import LocalPath
from framework.os_access.local_shell import local_shell
from framework.os_access.os_access_interface import OneWayPortMap, ReciprocalPortMap
from framework.os_access.posix_access import PosixAccess

_logger = logging.getLogger(__name__)


class LocalAccess(PosixAccess):

    @property
    def alias(self):
        return 'localhost'

    def is_accessible(self):
        return True

    def env_vars(self):
        return os.environ

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
        if not local_source_path.exists():
            raise CannotDownload("Local file {} doesn't exist.".format(local_source_path))
        try:
            os.symlink(str(local_source_path), str(destination))
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise
            raise AlreadyDownloaded(
                "Creating symlink {!s} pointing to {!s}".format(destination, local_source_path),
                destination)
        return destination

    @property
    def traffic_capture(self):
        raise NotImplementedError("Traffic capture on local machine is prohibited intentionally")

    def lock(self, path):
        return PortalockerLock(path)


local_access = LocalAccess()
