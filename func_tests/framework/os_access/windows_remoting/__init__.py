from __future__ import print_function

import subprocess

import pathlib2 as pathlib
import requests
import winrm

from framework.context_logger import ContextLogger
from framework.method_caching import cached_getter
from framework.os_access.command import DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.windows_remoting import _cmd, _powershell, registry, wmi

_logger = ContextLogger(__name__, 'winrm')


def command_args_to_str_list(command):
    command_str_list = []
    for arg in command:
        if isinstance(arg, str):
            command_str_list.append(arg)
        if isinstance(arg, (int, pathlib.PurePath)):
            command_str_list.append(str(arg))
    return command_str_list


class WinRM(object):
    """Windows-specific interface

    WinRM has only generic functions.
    WinRM must not know of particular WMI classes and CMD and PowerShell scripts.
    """

    def __init__(self, address, port, username, password):
        self._protocol = winrm.Protocol(
            'http://{}:{}/wsman'.format(address, port),
            username=username, password=password,
            transport='ntlm',  # 'plaintext' is easier to debug but, for some obscure reason, is slower.
            message_encryption='never',  # Any value except 'always' and 'auto'.
            operation_timeout_sec=120, read_timeout_sec=240)
        self._username = username
        self._repr = 'WinRM({!r}, {!r}, {!r}, {!r})'.format(address, port, username, password)
        self.wmi = wmi.Wmi(self._protocol)

    def __repr__(self):
        return self._repr

    def __del__(self):
        self._shell().__exit__(None, None, None)

    @cached_getter
    def _shell(self):
        """Lazy shell creation"""
        shell = _cmd.Shell(self._protocol)
        shell.__enter__()
        return shell

    def command(self, args):
        command_str_list = command_args_to_str_list(args)
        _logger.info("Command: %s", subprocess.list2cmdline(command_str_list))
        return self._shell().start(command_str_list, logger=_logger.getChild('cmd'))

    def run_command(self, command, input=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC):
        return self.command(command).run(input, timeout_sec=timeout_sec)

    def run_powershell_script(self, script, variables):
        return _powershell.run_powershell_script(self._shell(), script, variables, logger=_logger.getChild('cmd'))

    def is_working(self):
        try:
            self.run_command(['whoami'])
        except requests.RequestException:
            return False
        return True
