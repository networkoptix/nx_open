import binascii
import os
import time

import pytest
from typing import Any

from framework.os_access.command import Command, Run
from framework.os_access.local_shell import local_shell


@pytest.fixture(params=['local', 'ssh', 'ssh_terminal', 'windows'])
def command(request):  # type: (Any) -> Command
    if request.param == 'local':
        return local_shell.command(['cat'])
    if request.param == 'ssh':
        ssh = request.getfixturevalue('ssh')
        return ssh.command(['cat'])
    if request.param == 'ssh_terminal':
        ssh = request.getfixturevalue('ssh')
        return ssh.terminal_command(['cat'])
    if request.param == 'windows':
        winrm = request.getfixturevalue('winrm')
        return winrm.command(['more'])


@pytest.mark.parametrize('command', ['local', 'ssh_terminal', 'windows'], indirect=True)
def test_terminate(command):
    with command.running() as run:  # type: Run
        time.sleep(1)  # Allow command to warm up. Matters on Windows.
        run.terminate()
        stdout, stderr = run.communicate(timeout_sec=5)
        assert run.outcome.is_intended_termination
        # TODO: Pseudo-terminal echoes commands, and that's OK. Is there a way to leave command output only?
        # assert not stdout


@pytest.mark.parametrize('command', ['local', 'ssh', 'windows'], indirect=True)
def test_interaction(command):
    with command.running() as run:
        run.send(b'qwe\n')
        # TODO: Make `expect` method which expects bytes on stdout to avoid dumb waits.
        time.sleep(.1)  # Let command to receive and send data back.
        stdout, _ = run.receive(10)
        assert run.outcome is None
        assert stdout.rstrip(b'\r\n') == b'qwe'
        run.send(b'asd\n', is_last=True)
        time.sleep(.1)  # Let command to receive and send data back.
        stdout, _ = run.receive(10)
        assert run.outcome is not None
        assert run.outcome.is_success
        assert stdout.rstrip(b'\r\n') == b'asd'


@pytest.mark.parametrize('command', ['local', 'ssh', 'windows'], indirect=True)
def test_much_data_and_exit(command):
    data = binascii.hexlify(os.urandom(10000))
    with command.running() as run:
        stdout, stderr = run.communicate(input=data, timeout_sec=5000)
    assert stdout.rstrip(b'\r\n') == data
