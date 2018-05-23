import pytest
from pathlib2 import PurePath

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


def test_create_path(posix_shell):
    assert isinstance(make_ssh_path_cls(posix_shell)('/tmp'), PurePath)
