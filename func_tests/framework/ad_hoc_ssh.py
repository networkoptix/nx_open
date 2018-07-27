import logging
from contextlib import contextmanager
from threading import Thread

from cryptography.hazmat.backends import default_backend as crypto_default_backend
from cryptography.hazmat.primitives import serialization as crypto_serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from pathlib2 import Path

from framework.os_access.exceptions import Timeout, exit_status_error_cls
from framework.os_access.local_shell import local_shell
from framework.os_access.posix_shell_utils import sh_command_to_script

_logger = logging.getLogger(__name__)


def write_config(config, path):
    path.write_text(
        '\n'.join((
            '{key} {value}'.format(key=key, value=value)
            for key, value in config.items())).encode('ascii').decode('ascii'),
        encoding='ascii')


def generate_keys():
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


@contextmanager
def ad_hoc_sshd(try_port_range, dir, client_public_key, host_key):
    dir.mkdir(exist_ok=True, parents=True)
    host_key_path = dir / 'ad_hoc_host_key'
    host_key_path.write_text(host_key, encoding='ascii')
    host_key_path.chmod(0o600)
    authorized_keys_relative_path = dir / 'authorized_keys'
    authorized_keys_relative_path.write_text(client_public_key, encoding='ascii')
    host_config = {
        'ListenAddress': '127.0.0.1',
        'HostKey': host_key_path,
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
    host_config_path = dir / 'host_config'
    write_config(host_config, host_config_path)
    for port in try_port_range:
        sshd_command = ['/usr/sbin/sshd', '-ddd', '-f', str(host_config_path), '-p', str(port)]
        _logger.info('RUN: %s', sh_command_to_script(sshd_command))
        with local_shell.command(sshd_command).running() as process:  # Most tests are very quick.
            try:
                stdout, stderr = process.communicate(timeout_sec=1)
            except Timeout:
                _logger.debug("Assume it runs normally.")
            else:
                address_already_in_use = process.outcome.code == 255 and 'Address already in use' in stderr
                if not address_already_in_use:
                    raise exit_status_error_cls(process.outcome.code)(stdout, stderr)
                _logger.warning("Cannot bind to port: %d", port)
                continue
            thread = Thread(target=process.communicate, kwargs=dict(timeout_sec=300))
            thread.start()
            try:
                yield port
            finally:
                if process.outcome is None:
                    process.terminate()
                thread.join(630)
                _logger.info("Outcome of `sshd`: %r", process.outcome)
                assert not thread.is_alive()
            break
    else:
        raise EnvironmentError("Cannot bind to any port from {!r}".format(try_port_range))
