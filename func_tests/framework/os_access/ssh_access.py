import datetime
import timeit

import pytz
from pylru import lrudecorator

from framework.networking.linux import LinuxNetworking
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.posix_shell import SSH
from framework.os_access.ssh_path import make_ssh_path_cls
from framework.utils import RunningTime


class SSHAccess(OSAccess):
    def __init__(self, forwarded_ports, macs, username, key_path):
        self._macs = macs
        ssh_hostname, ssh_port = forwarded_ports['tcp', 22]
        self.ssh = SSH(ssh_hostname, ssh_port, username, key_path)
        self._forwarded_ports = forwarded_ports

    @property
    def forwarded_ports(self):
        return self._forwarded_ports

    @property
    def Path(self):
        return make_ssh_path_cls(self.ssh)

    def run_command(self, command, input=None):
        return self.ssh.run_command(command, input=input)

    def is_accessible(self):
        return self.ssh.is_working()

    @property
    @lrudecorator(1)
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
