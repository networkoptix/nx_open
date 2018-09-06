from __future__ import print_function

import logging
from subprocess import list2cmdline

import winrm
from pathlib2 import PurePath
from requests import RequestException

from framework.context_logger import ContextLogger
from framework.method_caching import cached_getter
from framework.os_access.command import DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.windows_remoting._cim_query import CIMClass
from ._cim_query import CIMQuery
from ._cmd import Shell
from ._powershell import run_powershell_script
from .registry import _WindowsRegistryKey

_logger = ContextLogger(__name__, 'winrm')


def command_args_to_str_list(command):
    command_str_list = []
    for arg in command:
        if isinstance(arg, str):
            command_str_list.append(arg)
        if isinstance(arg, (int, PurePath)):
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

    def __repr__(self):
        return self._repr

    def __del__(self):
        self._shell().__exit__(None, None, None)

    @cached_getter
    def _shell(self):
        """Lazy shell creation"""
        shell = Shell(self._protocol)
        shell.__enter__()
        return shell

    def command(self, args):
        command_str_list = command_args_to_str_list(args)
        _logger.info("Command: %s", list2cmdline(command_str_list))
        return self._shell().start(command_str_list, logger=_logger.getChild('cmd'))

    def run_command(self, command, input=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC):
        return self.command(command).check_output(input, timeout_sec=timeout_sec)

    def run_powershell_script(self, script, variables):
        return run_powershell_script(self._shell(), script, variables, logger=_logger.getChild('cmd'))

    def wmi_query(
            self,
            class_name, selectors,
            namespace=CIMClass.default_namespace, root_uri=CIMClass.default_root_uri):
        cim_class = CIMClass(class_name, namespace=namespace, root_uri=root_uri)
        cim_query = CIMQuery(self._protocol, cim_class, selectors)
        return cim_query

    def is_working(self):
        try:
            self.run_command(['whoami'])
        except RequestException:
            return False
        return True
