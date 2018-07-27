import getpass
import logging
from shutil import rmtree

import pytest
from pathlib2 import Path

from framework.ad_hoc_ssh import ad_hoc_sshd, generate_keys
from framework.os_access.ssh_shell import SSH
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)


@pytest.fixture(scope='session')
def ad_hoc_ssh_dir():
    path = Path.home() / '.func_tests/ad_hoc_ssh'
    rmtree(str(path), ignore_errors=True)
    path.mkdir(exist_ok=True)
    return path


@pytest.fixture(scope='session')
def ad_hoc_host_key():
    private_key, _ = generate_keys()
    return private_key


@pytest.fixture(scope='session')
def ad_hoc_client_key_pair():
    return generate_keys()


@pytest.fixture(scope='session')
def ad_hoc_client_public_key(ad_hoc_client_key_pair):
    _, public_key = ad_hoc_client_key_pair
    return public_key


@pytest.fixture(scope='session')
def ad_hoc_client_private_key(ad_hoc_client_key_pair):
    private_key, _ = ad_hoc_client_key_pair
    return private_key


@pytest.fixture()
def ad_hoc_ssh_port(service_ports, ad_hoc_ssh_dir, ad_hoc_client_public_key, ad_hoc_host_key):
    with ad_hoc_sshd(service_ports[0:5], ad_hoc_ssh_dir, ad_hoc_client_public_key, ad_hoc_host_key) as port:
        yield port


@pytest.fixture()
def ad_hoc_ssh(ad_hoc_ssh_port, ad_hoc_client_private_key):
    ssh = SSH('127.0.0.1', ad_hoc_ssh_port, getpass.getuser(), ad_hoc_client_private_key)
    _logger.debug(ssh)
    wait_for_true(ssh.is_working)
    yield ssh
    ssh.close()
