import time

import pytest

from framework.os_access.command import Command
from framework.os_access.local_shell import local_shell

pytest_plugins = ['fixtures.ad_hoc_ssh']


@pytest.fixture(params=['local', 'ssh_terminal', 'windows'])
def command_to_terminate(request):
    if request.param == 'local':
        return local_shell.command(['cat'])
    if request.param == 'ssh_terminal':
        ssh = request.getfixturevalue('ad_hoc_ssh')
        return ssh.terminal_command(['cat'])
    if request.param == 'windows':
        winrm = request.getfixturevalue('winrm')
        return winrm.command(['more'])


def test_terminate(command_to_terminate):
    with command_to_terminate as command:  # type: Command
        time.sleep(1)  # Allow command to warm up. Matters on Windows.
        command.terminate()
        exit_status, stdout, stderr = command.communicate(timeout_sec=5)
        # TODO: Pseudo-terminal echoes commands, and that's OK. Is there a way to leave command output only?
        # assert not stdout
        # assert 0 <= exit_status <= 255
