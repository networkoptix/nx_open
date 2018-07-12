def test_remote_process_interaction(winrm_shell):
    with winrm_shell.start('more').running() as run:
        for i in range(3):
            stdin = ('chunk %d\n' % i).encode('ascii')
            run.send(stdin)
            stdout, stderr = run.receive(None)
            assert stdout.replace(b'\r\n', b'\n') == stdin
        run.send(b'', is_last=True)
        _, _ = run.communicate()


def test_run_command(winrm_shell):
    with winrm_shell.start('echo', '123').running() as run:
        stdout_bytes, stderr_bytes = run.communicate()
    assert run.outcome.is_success
    assert stdout_bytes == b'123\r\n'
    assert stderr_bytes == b''


def test_run_command_with_stdin(winrm_shell):
    with winrm_shell.start('more').running() as run:
        stdout_bytes, stderr_bytes = run.communicate(b'123')
    assert run.outcome.is_success
    assert stdout_bytes == b'123\r\n'
    assert stderr_bytes == b''
