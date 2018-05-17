from pathlib2 import PurePath

from fixtures.ad_hoc_ssh import ad_hoc_ssh
from framework.os_access.posix_shell import SSH
from framework.os_access.ssh_path import make_ssh_path_cls

pytest_plugins = ['fixtures.ad_hoc_ssh']


def test_create(ad_hoc_ssh_server, ad_hoc_ssh_client_config):
    ssh_access = ad_hoc_ssh(ad_hoc_ssh_server, ad_hoc_ssh_client_config)
    assert isinstance(ssh_access, SSH)
    assert ssh_access.is_working()


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
