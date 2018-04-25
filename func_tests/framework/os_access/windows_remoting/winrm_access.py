from __future__ import print_function

import logging

import winrm
from requests import ReadTimeout

from framework.os_access.args import cmd_command_to_script
from framework.os_access.windows_remoting.cmd import Shell, run_command
from framework.os_access.windows_remoting.cmd.powershell import run_powershell_script

_logger = logging.getLogger(__name__)


class WinRMAccess(object):
    def __init__(self, hostname, port, username='Administrator', password='qweasd123'):
        self.protocol = winrm.Protocol(
            'http://{}:{}/wsman'.format(hostname, port),
            username=username, password=password,
            transport='ntlm')
        self.shell = Shell(self.protocol)
        self.shell.__enter__()

    def __del__(self):
        # Exit when object is garbage-collected.
        # There's no big problem if this is not called.
        self.shell.__exit__(None, None, None)

    def run_cmd_command(self, args, input=b''):
        _logger.debug("Command: %s", cmd_command_to_script(args))
        exit_code, stdout_bytes, stderr_bytes = run_command(self.shell, args, stdin_bytes=input)
        _logger.debug(
            "Outcome:\nexit code: %d\nstdout:\n%s\nstderr:\n%s",
            exit_code,
            stdout_bytes.decode('ascii', errors='replace'),
            stderr_bytes.decode('ascii', errors='replace'))
        return stdout_bytes

    def run_powershell_script(self, script, variables):
        return run_powershell_script(self.shell, script, variables)

    def is_working(self):
        try:
            self.run_cmd_command(['whoami'])
        except ReadTimeout:
            return False
        return True
