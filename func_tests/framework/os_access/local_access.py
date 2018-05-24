import datetime

import tzlocal

from framework.networking.prohibited import ProhibitedNetworking
from framework.os_access.local_path import LocalPath
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.posix_shell import local_shell


class _LocalPorts(object):
    def __getitem__(self, item):
        return '127.0.0.1', item


_local_ports = _LocalPorts()


class LocalAccess(OSAccess):
    def run_command(self, command, input=None):
        return local_shell.run_command(command, input=input)

    def is_accessible(self):
        return True

    @property
    def Path(self):
        return LocalPath

    @property
    def networking(self):
        return ProhibitedNetworking()

    @property
    def forwarded_ports(self):
        return _local_ports

    def get_time(self):
        local_timezone = tzlocal.get_localzone()
        local_datetime = datetime.datetime.now(tz=local_timezone)
        return local_datetime

    def set_time(self, new_time):
        raise NotImplementedError("Changing local time is prohibited")


local_access = LocalAccess()
