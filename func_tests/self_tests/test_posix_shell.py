import timeit

import pytest
from pathlib2 import PurePath

from framework.os_access.exceptions import Timeout, exit_status_error_cls
from framework.os_access.local_shell import local_shell
from framework.os_access.ssh_path import make_ssh_path_cls

pytest_plugins = ['fixtures.ad_hoc_ssh']


@pytest.fixture(params=['ssh', 'local'])
def posix_shell(request):
    if request.param == 'local':
        return local_shell
    if request.param == 'ssh':
        return request.getfixturevalue('ad_hoc_ssh')
    assert False


def test_run_command(posix_shell):
    given = 'It works!'
    received = posix_shell.run_command(['echo', '-n', given])
    assert received == given


def test_run_command_with_input(posix_shell):
    given = 'It works!'
    received = posix_shell.run_command(['cat'], input=given)
    assert received == given


def test_run_script(posix_shell):
    given = 'It works!'
    received = posix_shell.run_sh_script('echo -n ' "'" + given + "'")
    assert received == given


def test_non_zero_exit_code(posix_shell):
    random_exit_status = 42
    with pytest.raises(exit_status_error_cls(random_exit_status)):
        posix_shell.run_sh_script('exit {}'.format(random_exit_status))


def test_create_path(posix_shell):
    assert isinstance(make_ssh_path_cls(posix_shell)('/tmp'), PurePath)


def test_timeout(posix_shell):
    with pytest.raises(Timeout):
        posix_shell.run_command(['sleep', 60], timeout_sec=1)


# language=Python
_forked_child_script = '''
import os
import sys
import time

if __name__ == '__main__':
    sleep_time_sec = int(sys.argv[1])

    if os.fork():
        print("Parent process, finished.")
    else:
        print("Child process, sleeping for {:d} seconds...".format(sleep_time_sec))
        sys.stdout.flush()
        time.sleep(sleep_time_sec)
        print("Child process, finished.")
'''


def test_streams_left_open(posix_shell):
    sleep_time_sec = 60
    start_time = timeit.default_timer()
    posix_shell.run_command(['python', '-c', _forked_child_script, sleep_time_sec], timeout_sec=5)
    assert timeit.default_timer() - start_time < sleep_time_sec


# language=Python
_early_closing_streams_script = '''
import sys
import time

if __name__ == '__main__':
    sleep_time_sec = int(sys.argv[1])
    sys.stdout.write("Test data on stdout.")
    sys.stderr.write("Test data on stderr.")
    sys.stdout.close()
    sys.stderr.close()
    time.sleep(sleep_time_sec)
'''


def test_streams_closed_early_but_process_timed_out(posix_shell):
    with pytest.raises(Timeout):
        posix_shell.run_command(['python', '-c', _early_closing_streams_script, 5], timeout_sec=2)


def test_streams_closed_early_and_process_on_time(posix_shell):
    posix_shell.run_command(['python', '-c', _early_closing_streams_script, 2], timeout_sec=5)


# language=Python
_wait_for_any_data_script = r'''
import sys

sys.stdin.read(1)
'''


def test_receive_times_out(posix_shell):
    acceptable_error_sec = 0.1
    timeout_sec = 2
    command = posix_shell.command(['python', '-c', _wait_for_any_data_script], set_eux=False)
    with command.running() as run:
        begin = timeit.default_timer()
        output_line, stderr = run.receive(timeout_sec)
        end = timeit.default_timer()
        assert stderr == b''
        assert output_line == b''
        assert timeout_sec < end - begin < timeout_sec + acceptable_error_sec
        _, _ = run.communicate(input=b' ', timeout_sec=acceptable_error_sec)
    assert run.outcome.is_success


# language=Python
_delayed_cat = r'''
import sys
import time

if __name__ == '__main__':
    sleep_time_sec = float(sys.argv[1])
    while True:
        line = sys.stdin.readline()
        if line == '\n':
            break
        time.sleep(sleep_time_sec)
        sys.stdout.write(line)
        sys.stdout.flush()
'''


def test_receive_with_delays(posix_shell):
    delay_sec = 2
    acceptable_error_sec = 0.1
    timeout_tolerance_sec = 0.2
    with posix_shell.command(['python', '-c', _delayed_cat, delay_sec], set_eux=False).running() as run:
        second_begin = timeit.default_timer()
        input_line = b'test line\n'
        run.send(input_line)
        second_output_line, second_stderr = run.receive(delay_sec + timeout_tolerance_sec)
        second_check = timeit.default_timer()
        assert second_stderr == b''
        assert second_output_line == input_line
        assert delay_sec < second_check - second_begin < delay_sec + acceptable_error_sec
        run.communicate(b'\n', delay_sec + timeout_tolerance_sec)
    assert run.outcome.is_success
