import timeit

import pytest
from pathlib2 import PurePath

from framework.os_access.exceptions import Timeout, exit_status_error_cls
from framework.os_access.posix_shell import local_shell
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
        posix_shell.run_command(['sleep', 5], timeout_sec=1)


# language=Python
_forked_child_script = '''
from __future__ import print_function

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
    sleep_time_sec = 10
    start_time = timeit.default_timer()
    posix_shell.run_command(['python', '-c', _forked_child_script, sleep_time_sec], timeout_sec=5)
    assert timeit.default_timer() - start_time < sleep_time_sec
