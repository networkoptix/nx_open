from pylru import lrudecorator

from framework.networking.windows import WindowsNetworking
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.smb_path import SMBPath
from framework.os_access.windows_remoting import WinRM


class WindowsAccess(OSAccess):
    """Run CMD and PowerShell and access CIM/WMI via WinRM, access filesystem via SMB"""

    def __init__(self, forwarded_ports, macs, username, password):
        self.macs = macs
        self._username = username
        self._password = password
        winrm_address, winrm_port = forwarded_ports['tcp', 5985]
        self.winrm = WinRM(winrm_address, winrm_port, username, password)
        self._forwarded_ports = forwarded_ports

    @property
    def forwarded_ports(self):
        return self._forwarded_ports

    @property
    def Path(self):
        winrm = self.winrm

        class SpecificSMBPath(SMBPath):
            _username = self._username
            _password = self._password
            _name_service_address, _name_service_port = self.forwarded_ports['udp', 137]
            _session_service_address, _session_service_port = self.forwarded_ports['tcp', 139]

            @classmethod
            def tmp(cls):
                env_vars = winrm.user_env_vars()
                return cls(env_vars[u'TEMP']) / 'FuncTests'

            @classmethod
            def home(cls):
                env_vars = winrm.user_env_vars()
                return cls(env_vars[u'USERPROFILE'])

        return SpecificSMBPath

    def run_command(self, command, input=None):
        self.winrm.run_command(command, input=input)

    def is_accessible(self):
        return self.winrm.is_working()

    @property
    @lrudecorator(1)
    def networking(self):
        return WindowsNetworking(self.winrm, self.macs)
