from __future__ import print_function

import logging
from subprocess import list2cmdline

import winrm
from requests import ReadTimeout

from framework.os_access.windows_remoting.cmd import Shell, run_command

_logger = logging.getLogger(__name__)


class WinRMAccess(object):
    def __init__(self, hostname, port, username='Administrator', password='qweasd123'):
        self._protocol = winrm.Protocol(
            'http://{}:{}/wsman'.format(hostname, port),
            username=username, password=password,
            transport='ntlm')

    def run_command(self, args, input=b''):
        _logger.debug("Command: %s", list2cmdline(args))
        with Shell(self._protocol) as shell:
            exit_code, stdout_bytes, stderr_bytes = run_command(shell, args, stdin_bytes=input)
        _logger.debug(
            "Outcome:\nexit code: %d\nstdout:\n%s\nstderr:\n%s",
            exit_code,
            stdout_bytes.decode('ascii', errors='replace'),
            stderr_bytes.decode('ascii', errors='replace'))
        return stdout_bytes

    def is_working(self):
        try:
            self.run_command(['whoami'])
        except ReadTimeout:
            return False
        return True
