import logging
from subprocess import PIPE, Popen, call, check_call, check_output

import pytest

from framework.ca import CA

_logger = logging.getLogger(__name__)


@pytest.fixture()
def temp_ca(node_dir):
    return CA(node_dir / 'ca')


@pytest.fixture()
def gen_dir(node_dir):
    path = node_dir / 'gen'
    path.mkdir(parents=True, exist_ok=True)
    return path


@pytest.fixture()
def key_and_cert_path(temp_ca, gen_dir):
    contents = temp_ca.generate_key_and_cert()
    path = gen_dir / 'latest.pem'
    # CA saves files too but not as part of documented interface.
    path.write_text(contents)
    return path


def test_cert_is_signed_by_ca(temp_ca, key_and_cert_path):
    check_call(['openssl', 'verify', '-CAfile', str(temp_ca.cert_path), str(key_and_cert_path)])


def test_key_and_cert_match(key_and_cert_path):
    # Modulus and public exponent must match.
    # Public exponent is hard to retrieve and almost always 65537.
    key_modulus = check_output(['openssl', 'rsa', '-modulus', '-in', str(key_and_cert_path), '-noout'])
    cert_modulus = check_output(['openssl', 'x509', '-modulus', '-in', str(key_and_cert_path), '-noout'])
    assert key_modulus == cert_modulus


def test_real_ca_cert_is_valid(ca):
    assert call(['openssl', 'x509', '-text', '-noout', '-in', str(ca.cert_path)]) == 0, "Cannot read certificate."
    assert call(['openssl', 'x509', '-checkend', '0', '-in', str(ca.cert_path)]) == 0, "Certificate is expired!"
    check_end_process = Popen(['openssl', '-checkend', str(14 * 24 * 3600), str(ca.cert_path)], stdout=PIPE)
    check_end_process_out = check_end_process.communicate()
    if check_end_process_out != 0:
        _logger.warning("Certificate will expire in less than 2 weeks at: %s", check_end_process_out)
