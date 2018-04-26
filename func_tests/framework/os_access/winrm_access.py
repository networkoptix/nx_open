from __future__ import print_function

import logging

import winrm
from pylru import lrudecorator
from requests import ReadTimeout

from framework.os_access.args import cmd_command_to_script
from framework.os_access.smb_path import SMBPath
from framework.os_access.windows_remoting.cmd import Shell, run_command
from framework.os_access.windows_remoting.cmd.powershell import run_powershell_script
from framework.os_access.windows_remoting.dot_net.users import get_user

_logger = logging.getLogger(__name__)


class WinRMAccess(object):
    def __init__(self, ports, username=u'Administrator', password=u'qweasd123'):
        winrm_address, winrm_port = ports['tcp', 5985]
        self.protocol = winrm.Protocol(
            'http://{}:{}/wsman'.format(winrm_address, winrm_port),
            username=username, password=password,
            transport='ntlm')

        @lrudecorator(1)
        def user_info():
            return get_user(self.protocol, username)

        # TODO: Consider another architecture.
        class _SMBPath(SMBPath):
            _username = username
            _password = password
            _name_service_address, _name_service_port = ports['udp', 137]
            _session_service_address, _session_service_port = ports['tcp', 139]

            @classmethod
            def tmp(cls):
                _, _, env_vars = user_info()
                return cls(env_vars[u'TEMP']) / 'FuncTests'

            @classmethod
            def home(cls):
                _, _, env_vars = user_info()
                return cls(env_vars[u'USERPROFILE'])

        self.Path = _SMBPath

    def __del__(self):
        # Exit when object is garbage-collected.
        # There's no big problem if this is not called.
        self._shell().__exit__(None, None, None)

    @lrudecorator(1)
    def _shell(self):
        """Lazy shell creation"""
        shell = Shell(self.protocol)
        shell.__enter__()
        return shell

    def run_cmd_command(self, args, input=b''):
        _logger.debug("Command: %s", cmd_command_to_script(args))
        exit_code, stdout_bytes, stderr_bytes = run_command(self._shell(), args, stdin_bytes=input)
        _logger.debug(
            "Outcome:\nexit code: %d\nstdout:\n%s\nstderr:\n%s",
            exit_code,
            stdout_bytes.decode('ascii', errors='replace'),
            stderr_bytes.decode('ascii', errors='replace'))
        return stdout_bytes

    def run_powershell_script(self, script, variables):
        return run_powershell_script(self._shell(), script, variables)

    def is_working(self):
        try:
            self.run_cmd_command(['whoami'])
        except ReadTimeout:
            return False
        return True
