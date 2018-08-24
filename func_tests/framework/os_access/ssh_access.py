import datetime
import timeit

import pytz

from framework.lock import PosixShellFileLock
from framework.method_caching import cached_property
from framework.networking.linux import LinuxNetworking
from framework.networking.prohibited import ProhibitedNetworking
from framework.os_access.os_access_interface import OneWayPortMap, ReciprocalPortMap
from framework.os_access.posix_access import PosixAccess
from framework.os_access.posix_shell_path import PosixShellPath
from framework.os_access.remote_access import RemoteAccess
from framework.os_access.ssh_shell import SSH
from framework.os_access.ssh_traffic_capture import SSHTrafficCapture
from framework.utils import RunningTime


class SSHAccess(RemoteAccess, PosixAccess):

    def __init__(self, host_alias, port_map, username, key):
        RemoteAccess.__init__(self, host_alias, port_map)
        self.key = key
        self.username = username

    @cached_property
    def ssh(self):
        return SSH(
            self.port_map.remote.address, self.port_map.remote.tcp(22),
            self.username, self.key)

    @property
    def shell(self):
        return self.ssh

    @cached_property
    def Path(self):
        return PosixShellPath.specific_cls(self.ssh)

    def is_accessible(self):
        return self.ssh.is_working()

    def get_time(self):
        started_at = timeit.default_timer()
        timestamp_output = self.run_command(['date', '+%s'])
        delay_sec = timeit.default_timer() - started_at
        timestamp = int(timestamp_output.rstrip())
        timezone_name = self.Path('/etc/timezone').read_text().rstrip()
        timezone = pytz.timezone(timezone_name)
        local_time = datetime.datetime.fromtimestamp(timestamp, tz=timezone)
        return RunningTime(local_time, datetime.timedelta(seconds=delay_sec))

    def lock(self, path):
        return PosixShellFileLock(self.shell, path)


class VmSshAccess(SSHAccess):

    def __init__(self, host_alias, port_map, macs, username, key):
        super(VmSshAccess, self).__init__(host_alias, port_map, username, key)
        self._macs = macs

    def __repr__(self):
        return '<VmSshAccess via {!r}>'.format(self.ssh)

    @cached_property
    def networking(self):
        return LinuxNetworking(self.ssh, self._macs)

    def set_time(self, new_time):
        started_at = datetime.datetime.now(pytz.utc)
        self.run_command(['date', '--set', new_time.isoformat()])
        return RunningTime(new_time, datetime.datetime.now(pytz.utc) - started_at)

    @cached_property
    def traffic_capture(self):
        return SSHTrafficCapture(self.shell, self.Path.tmp() / 'traffic_capture')


class PhysicalSshAccess(SSHAccess):

    def __init__(self, host_alias, address, username, key):
        # portmap.local is never used, so OneWayPortMap.local() for it will be ok here
        port_map = ReciprocalPortMap(OneWayPortMap.direct(address), OneWayPortMap.local())
        super(PhysicalSshAccess, self).__init__(host_alias, port_map, username, key)

    def __repr__(self):
        return '<PhysicalSshAccess via {!r}>'.format(self.ssh)

    @property
    def networking(self):
        return ProhibitedNetworking()

    def set_time(self, new_time):
        raise NotImplementedError("Changing time on physical is prohibited")

    @cached_property
    def traffic_capture(self):
        raise NotImplementedError("Traffic capture on physical machine is prohibited intentionally")
