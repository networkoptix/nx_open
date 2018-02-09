import logging
import shutil

import pytest
from subprocess import check_call, check_output, call, Popen, PIPE

from test_utils.ca import CA

_logger = logging.getLogger(__name__)


@pytest.fixture
def temp_ca(run_options):
    ca_self_test_dir = run_options.work_dir / 'ca_self_test'
    shutil.rmtree(str(ca_self_test_dir), ignore_errors=True)
    return CA(ca_self_test_dir)


@pytest.fixture
def gen_dir(run_options):
    path = run_options.work_dir / 'ca_self_test_gen'
    path.mkdir(parents=True, exists_ok=True)
    return path


@pytest.fixture
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


@pytest.fixture
def real_ca(run_options):
    return CA(run_options.work_dir / 'ca')


def test_real_ca_cert_is_valid(real_ca):
    assert call(['openssl', 'x509', '-text', '-noout', '-in', str(real_ca.cert_path)]) == 0, "Cannot read certificate."
    assert call(['openssl', 'x509', '-checkend', '0', '-in', str(real_ca.cert_path)]) == 0, "Certificate is expired!"
    check_end_process = Popen(['openssl', '-checkend', str(14 * 24 * 3600), str(real_ca.cert_path)], stdout=PIPE)
    check_end_process_out = check_end_process.communicate()
    if check_end_process_out != 0:
        _logger.warning("Certificate will expire in less than 2 weeks at: %s", check_end_process_out)
