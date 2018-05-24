import getpass
import logging
import subprocess
from collections import namedtuple
from shutil import rmtree
from threading import Thread

import pytest
from cryptography.hazmat.backends import default_backend as crypto_default_backend
from cryptography.hazmat.primitives import serialization as crypto_serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from netaddr.ip import IPAddress
from pathlib2 import Path

from framework.os_access.posix_shell import SSH
from framework.os_access.posix_shell_utils import sh_command_to_script
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
    return private_key, public_key


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
    host_private_key_path.write_bytes(ad_hoc_host_key)
    host_private_key_path.chmod(0o600)
    authorized_keys_relative_path = ad_hoc_ssh_dir / 'authorized_keys'
    authorized_keys_relative_path.write_bytes(ad_hoc_client_public_key)
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
    sshd_command = ['/usr/sbin/sshd', '-D', '-d', '-f', str(host_config_path), '-p', str(port)]
    _logger.info('RUN: %s', sh_command_to_script(sshd_command))
    process = subprocess.Popen(sshd_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def dump_output():
        while process.poll() is None:
            _logger.debug('STDOUT:\n%s', process.stdout.read())
            _logger.debug('STDERR:\n%s', process.stderr.read())

    Thread(target=dump_output).start()

    yield _SSHHost(ip, port)

    process.terminate()  # Hangs because of SSH client connection pool.


@pytest.fixture()
def ad_hoc_ssh(ad_hoc_ssh_dir, ad_hoc_ssh_server, ad_hoc_client_private_key):
    client_key_path = ad_hoc_ssh_dir / 'client_key'
    client_key_path.write_bytes(ad_hoc_client_private_key)
    client_key_path.chmod(0o600)
    ssh = SSH(ad_hoc_ssh_server.hostname, ad_hoc_ssh_server.port, getpass.getuser(), client_key_path)
    _logger.debug(ssh)
    wait_for_true(ssh.is_working)
    return ssh
