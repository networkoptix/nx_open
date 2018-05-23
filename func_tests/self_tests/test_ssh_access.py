from pathlib2 import PurePath

from framework.os_access.ssh_path import make_ssh_path_cls

pytest_plugins = ['fixtures.ad_hoc_ssh']


def test_run_command(ad_hoc_ssh):
    given = 'It works!'
    received = ad_hoc_ssh.run_command(['echo', '-n', given])
    assert received == given


def test_run_command_with_input(ad_hoc_ssh):
    given = 'It works!'
    received = ad_hoc_ssh.run_command(['cat'], input=given)
    assert received == given


def test_run_script(ad_hoc_ssh):
    given = 'It works!'
    received = ad_hoc_ssh.run_sh_script('echo -n ' "'" + given + "'")
    assert received == given


def test_create_path(ad_hoc_ssh):
    assert isinstance(make_ssh_path_cls(ad_hoc_ssh)('/tmp'), PurePath)
