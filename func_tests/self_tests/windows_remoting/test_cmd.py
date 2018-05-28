import logging

import pytest

from framework.os_access.windows_remoting._cmd import Shell, receive_stdout_and_stderr_until_done, run_command

log = logging.getLogger(__name__)


def test_remote_process_interaction(shell):
    with shell.start('more') as command:
        for i in range(3):
            stdin = ('chunk %d\n' % i).encode('ascii')
            command.send_stdin(stdin)
            stdout, stderr = command.receive_stdout_and_stderr()
            assert stdout.replace(b'\r\n', b'\n') == stdin
        command.send_stdin(b'', end=True)
        _, _ = receive_stdout_and_stderr_until_done(command)


def test_run_command(shell):
    exit_code, stdout_bytes, stderr_bytes = run_command(shell, ['echo', '123'])
    assert exit_code == 0
    assert stdout_bytes == b'123\r\n'
    assert stderr_bytes == b''


def test_run_command_with_stdin(shell):
    exit_code, stdout_bytes, stderr_bytes = run_command(shell, ['more'], b'123')
    assert exit_code == 0
    assert stdout_bytes == b'123\r\n'
    assert stderr_bytes == b''
