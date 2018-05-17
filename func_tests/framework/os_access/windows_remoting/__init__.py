from __future__ import print_function

import logging
from subprocess import list2cmdline

import winrm
from pathlib2 import PurePath
from pylru import lrudecorator
from requests import RequestException

from ._cim_query import CIMQuery
from ._cmd import Shell, run_command
from ._env_vars import EnvVars
from ._powershell import run_powershell_script
from ._registry import Key
from ._users import Users

_logger = logging.getLogger(__name__)


def command_args_to_str_list(command):
    command_str_list = []
    for arg in command:
        if isinstance(arg, str):
            command_str_list.append(arg)
        if isinstance(arg, (int, PurePath)):
            command_str_list.append(str(arg))
    return command_str_list


class WinRM(object):
    """Windows-specific interface"""

    def __init__(self, address, port, username, password):
        self._protocol = winrm.Protocol(
            'http://{}:{}/wsman'.format(address, port),
            username=username, password=password,
            transport='plaintext',  # 'ntlm' works fast, 'plaintext' is easier to debug.
            operation_timeout_sec=120, read_timeout_sec=240)
        self._username = username

    def __del__(self):
        self._shell().__exit__(None, None, None)

    @lrudecorator(1)
    def _shell(self):
        """Lazy shell creation"""
        shell = Shell(self._protocol)
        shell.__enter__()
        return shell

    def run_command(self, command, input=b''):
        command_str_list = command_args_to_str_list(command)
        _logger.debug("Command: %s", list2cmdline(command_str_list))
        exit_code, stdout_bytes, stderr_bytes = run_command(self._shell(), command_str_list, stdin_bytes=input)
        _logger.debug(
            "Outcome:\nexit code: %d\nstdout:\n%s\nstderr:\n%s",
            exit_code,
            stdout_bytes.decode('ascii', errors='replace'),
            stderr_bytes.decode('ascii', errors='replace'))
        return stdout_bytes

    def run_powershell_script(self, script, variables):
        return run_powershell_script(self._shell(), script, variables)

    @lrudecorator(1)
    def user_env_vars(self):
        users = Users(self._protocol)
        account = users.account_by_name(self._username)
        profile = users.profile_by_sid(account[u'SID'])
        profile_dir = profile[u'LocalPath']
        default_env_vars = {
            u'USERPROFILE': profile_dir,
            u'PROGRAMFILES': u'C:\\Program Files',
            }
        env_vars = EnvVars.request(self._protocol, account[u'Caption'], default_env_vars)
        return env_vars

    @lrudecorator(1)
    def system_profile_dir(self):
        users = Users(self._protocol)
        system_profile = users.system_profile()
        profile_dir = system_profile[u'LocalPath']
        return profile_dir

    def wmi_query(self, class_name, selectors):
        return CIMQuery(self._protocol, class_name, selectors)

    def registry_key(self, path):
        return Key(self.wmi_query(u'StdRegProv', {}), path)

    def is_working(self):
        try:
            self.run_command(['whoami'])
        except RequestException:
            return False
        return True
