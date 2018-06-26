def test_remote_process_interaction(winrm_shell):
    with winrm_shell.start('more') as command:
        for i in range(3):
            stdin = ('chunk %d\n' % i).encode('ascii')
            command.send(stdin)
            _, stdout, stderr = command.receive(None)
            assert stdout.replace(b'\r\n', b'\n') == stdin
        command.send(b'', is_last=True)
        _, _, _ = command.communicate()


def test_run_command(winrm_shell):
    with winrm_shell.start('echo', '123') as command:
        exit_code, stdout_bytes, stderr_bytes = command.communicate()
    assert exit_code == 0
    assert stdout_bytes == b'123\r\n'
    assert stderr_bytes == b''


def test_run_command_with_stdin(winrm_shell):
    with winrm_shell.start('more') as command:
        exit_code, stdout_bytes, stderr_bytes = command.communicate(b'123')
    assert exit_code == 0
    assert stdout_bytes == b'123\r\n'
    assert stderr_bytes == b''
