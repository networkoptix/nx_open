from pylru import lrudecorator

from framework.installation.deb_installation import DebInstallation
from framework.networking.linux import LinuxNetworking
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.posix_shell import SSH
from framework.os_access.ssh_path import make_ssh_path_cls


class SSHAccess(OSAccess):
    def __init__(self, forwarded_ports, macs):  # TODO: Provide username and password as well.
        self._macs = macs
        ssh_hostname, ssh_port = forwarded_ports['tcp', 22]
        self.ssh = SSH(ssh_hostname, ssh_port)
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
