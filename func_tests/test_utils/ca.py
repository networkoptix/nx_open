import logging
import os

from subprocess import check_call
from textwrap import dedent

log = logging.getLogger(__name__)


class CA(object):
    """CertificateAuthority """

    def __init__(self, ca_dir):
        if not os.path.exists(ca_dir):
            os.makedirs(ca_dir)
        self.cert_path = os.path.join(ca_dir, 'ca.crt')
        self._key_path = os.path.join(ca_dir, 'ca.key')
        self._serial_path = os.path.join(ca_dir, 'ca.srl')
        self._extensions_config_path = os.path.join(ca_dir, 'ca.extensions.cnf')
        if not os.path.exists(self.cert_path) or not os.path.exists(self._key_path):
            log.debug("Generating new CA certificate %s and key %s...", self.cert_path, self._key_path)
            check_call([
                'openssl', 'req', '-x509',
                '-newkey', 'rsa:2048', '-nodes', '-keyout', self._key_path,
                '-subj', '/C=US/O=NetworkOptix/CN=nxca',
                '-out', self.cert_path])
            log.info("New CA certificate %s can be added to browser or system as trusted.", self.cert_path)
        if not os.path.exists(self._extensions_config_path):
            log.debug("Save extensions config %s...", self._extensions_config_path)
            with open(self._extensions_config_path, 'w') as extensions_config_file:
                extensions_config_file.write(str.strip(dedent("""
                    basicConstraints = CA:FALSE
                    keyUsage = keyEncipherment
                    subjectAltName = @alt_names
                    [alt_names]
                    DNS.1 = localhost
                    IP.1 = 127.0.0.1
                """)))
        if not os.path.exists(self._serial_path):
            log.debug("Saving new serial file %s...", self._serial_path)
            with open(self._serial_path, 'w') as serial_file:
                serial_file.write('01')  # Could be any hex-encoded byte string up to 20 octets.
        self._gen_dir = os.path.join(ca_dir, 'gen')
        if not os.path.exists(self._gen_dir):
            os.makedirs(self._gen_dir)

    def generate_key_and_cert(self):
        log.debug("Generating key...")
        with open(self._serial_path) as serial_file:
            basename = os.path.join(self._gen_dir, serial_file.read().rstrip())
        key_path = basename + '.key'
        check_call(['openssl', 'genrsa', '-out', key_path, '2048'])
        log.debug("Making request...")
        subject = '/C=US/O=NetworkOptix/CN=nxwitness'
        request_path = basename + '.csr'
        check_call(['openssl', 'req', '-new', '-key', key_path, '-subj', subject, '-out', request_path])
        log.debug("Signing request...")
        cert_path = basename + '.crt'
        check_call([
            'openssl', 'x509', '-req',
            '-CA', self.cert_path, '-CAkey', self._key_path, '-CAserial', self._serial_path, '-CAcreateserial',
            '-in', request_path, '-out', cert_path,
            '-extfile', self._extensions_config_path])
        with open(key_path) as key_file, open(cert_path) as cert_file:
            return key_file.read() + cert_file.read()
