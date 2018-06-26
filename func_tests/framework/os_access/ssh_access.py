import datetime
import timeit

import pytz

from framework.method_caching import cached_property
from framework.networking.linux import LinuxNetworking
from framework.os_access.posix_access import PosixAccess
from framework.os_access.ssh_shell import SSH
from framework.os_access.remote_access import RemoteAccess
from framework.os_access.ssh_path import make_ssh_path_cls
from framework.utils import RunningTime


class SSHAccess(RemoteAccess, PosixAccess):
    def __init__(self, port_map, macs, username, key_path):
        RemoteAccess.__init__(self, port_map)
        self._macs = macs
        self.ssh = SSH(port_map.remote.address, port_map.remote.tcp(22), username, key_path)

    def __repr__(self):
        return '<SSHAccess via {!r}>'.format(self.ssh)

    @property
    def shell(self):
        return self.ssh

    @cached_property
    def Path(self):
        return make_ssh_path_cls(self.ssh)

    def is_accessible(self):
        return self.ssh.is_working()

    @cached_property
    def networking(self):
        return LinuxNetworking(self.ssh, self._macs)

    def get_time(self):
        started_at = timeit.default_timer()
        timestamp_output = self.run_command(['date', '+%s'])
        delay_sec = timeit.default_timer() - started_at
        timestamp = int(timestamp_output.rstrip())
        timezone_name = self.Path('/etc/timezone').read_text().rstrip()
        timezone = pytz.timezone(timezone_name)
        local_time = datetime.datetime.fromtimestamp(timestamp, tz=timezone)
        return RunningTime(local_time, datetime.timedelta(seconds=delay_sec))

    def set_time(self, new_time):
        started_at = datetime.datetime.now(pytz.utc)
        self.run_command(['date', '--set', new_time.isoformat()])
        return RunningTime(new_time, datetime.datetime.now(pytz.utc) - started_at)
