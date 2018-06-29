import time

import pytest

from framework.os_access.command import Command
from framework.os_access.local_shell import local_shell

pytest_plugins = ['fixtures.ad_hoc_ssh']


@pytest.fixture(params=['local', 'ssh', 'ssh_terminal', 'windows'])
def command(request):
    if request.param == 'local':
        return local_shell.command(['cat'])
    if request.param == 'ssh':
        ssh = request.getfixturevalue('ad_hoc_ssh')
        return ssh.command(['cat'])
    if request.param == 'ssh_terminal':
        ssh = request.getfixturevalue('ad_hoc_ssh')
        return ssh.terminal_command(['cat'])
    if request.param == 'windows':
        winrm = request.getfixturevalue('winrm')
        return winrm.command(['more'])


@pytest.mark.parametrize('command', ['local', 'ssh_terminal', 'windows'], indirect=True)
def test_terminate(command):
    with command as command:  # type: Command
        time.sleep(1)  # Allow command to warm up. Matters on Windows.
        command.terminate()
        outcome, stdout, stderr = command.communicate(timeout_sec=5)
        assert outcome.is_intended_termination
        # TODO: Pseudo-terminal echoes commands, and that's OK. Is there a way to leave command output only?
        # assert not stdout


@pytest.mark.parametrize('command', ['local', 'ssh', 'windows'], indirect=True)
def test_interaction(command):
    with command:
        command.send(b'qwe\n')
        # TODO: Make `expect` method which expects bytes on stdout to avoid dumb waits.
        time.sleep(.1)  # Let command to receive and send data back.
        outcome, stdout, _ = command.receive(10)
        assert outcome is None
        assert stdout.rstrip(b'\r\n') == b'qwe'
        command.send(b'asd\n', is_last=True)
        time.sleep(.1)  # Let command to receive and send data back.
        outcome, stdout, _ = command.receive(10)
        assert outcome is not None
        assert outcome.is_success
        assert stdout.rstrip(b'\r\n') == b'asd'
