import getpass
import logging
from collections import namedtuple
from shutil import rmtree
from threading import Thread

import pytest
from cryptography.hazmat.backends import default_backend as crypto_default_backend
from cryptography.hazmat.primitives import serialization as crypto_serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from netaddr.ip import IPAddress
from pathlib2 import Path

from framework.os_access.local_shell import local_shell
from framework.os_access.posix_shell_utils import sh_command_to_script
from framework.os_access.ssh_shell import SSH
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)


def _generate_keys():
    """See: https://stackoverflow.com/a/39126754/1833960"""
    key = rsa.generate_private_key(
        backend=crypto_default_backend(),
        public_exponent=65537,
        key_size=2048
        )
    private_key = key.private_bytes(
        crypto_serialization.Encoding.PEM,
        crypto_serialization.PrivateFormat.TraditionalOpenSSL,  # Paramiko accepts '-----BEGIN RSA PRIVATE KEY-----".
        crypto_serialization.NoEncryption())
    public_key = key.public_key().public_bytes(
        crypto_serialization.Encoding.OpenSSH,
        crypto_serialization.PublicFormat.OpenSSH
        )
    return private_key.decode('ascii'), public_key.decode('ascii')


def _write_config(config, path):
    path.write_text(
        '\n'.join((
            '{key} {value}'.format(key=key, value=value)
            for key, value in config.items())).encode('ascii').decode('ascii'),
        encoding='ascii')


@pytest.fixture(scope='session')
def ad_hoc_ssh_dir():
    path = Path.home() / '.func_tests/ad_hoc_ssh'
    rmtree(str(path), ignore_errors=True)
    path.mkdir(exist_ok=True)
    return path


@pytest.fixture(scope='session')
def ad_hoc_host_key():
    private_key, _ = _generate_keys()
    return private_key


@pytest.fixture(scope='session')
def ad_hoc_client_key_pair():
    return _generate_keys()


@pytest.fixture(scope='session')
def ad_hoc_client_public_key(ad_hoc_client_key_pair):
    _, public_key = ad_hoc_client_key_pair
    return public_key


@pytest.fixture(scope='session')
def ad_hoc_client_private_key(ad_hoc_client_key_pair):
    private_key, _ = ad_hoc_client_key_pair
    return private_key


_SSHHost = namedtuple('SSHHost', ['hostname', 'port'])


@pytest.fixture()
def ad_hoc_ssh_server(ad_hoc_ssh_dir, ad_hoc_client_public_key, ad_hoc_host_key):
    ip = IPAddress('127.0.0.1')
    port = 60022
    host_private_key_path = ad_hoc_ssh_dir / 'ad_hoc_host_key'
    host_private_key_path.write_text(ad_hoc_host_key, encoding='ascii')
    host_private_key_path.chmod(0o600)
    authorized_keys_relative_path = ad_hoc_ssh_dir / 'authorized_keys'
    authorized_keys_relative_path.write_text(ad_hoc_client_public_key, encoding='ascii')
    host_config = {
        'ListenAddress': ip,
        'HostKey': host_private_key_path,
        'AuthorizedKeysFile': authorized_keys_relative_path.relative_to(Path.home()),
        'LogLevel': 'VERBOSE',
        'PubkeyAuthentication': 'yes',
        'PasswordAuthentication': 'no',
        'ChallengeResponseAuthentication': 'no',
        'KerberosAuthentication': 'no',
        'GSSAPIAuthentication': 'no',
        'UsePAM': 'no',
        'UseDNS': 'no',
        }
    host_config_path = ad_hoc_ssh_dir / 'host_config'
    _write_config(host_config, host_config_path)
    sshd_command = ['/usr/sbin/sshd', '-ddd', '-f', str(host_config_path), '-p', str(port)]
    _logger.info('RUN: %s', sh_command_to_script(sshd_command))
    with local_shell.command(sshd_command).running() as process:  # Most tests are very quick.
        thread = Thread(target=process.communicate, kwargs=dict(timeout_sec=300))
        thread.start()
        try:
            yield _SSHHost(ip, port)
        finally:
            thread.join(630)
            assert not thread.is_alive()


@pytest.fixture()
def ad_hoc_ssh(ad_hoc_ssh_server, ad_hoc_client_private_key):
    ssh = SSH(ad_hoc_ssh_server.hostname, ad_hoc_ssh_server.port, getpass.getuser(), ad_hoc_client_private_key)
    _logger.debug(ssh)
    wait_for_true(ssh.is_working)
    yield ssh
    ssh.close()
