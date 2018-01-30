import logging

from subprocess import check_call
from textwrap import dedent

log = logging.getLogger(__name__)


class CA(object):
    """CertificateAuthority """

    def __init__(self, ca_dir):
        self.cert_path = ca_dir / 'ca.crt'
        self._key_path = ca_dir / 'ca.key'
        self._serial_path = ca_dir / 'ca.srl'
        self._extensions_config_path = ca_dir / 'ca.extensions.cnf'
        if not self.cert_path.exists() or not self._key_path.exists():
            log.debug("Generating new CA certificate %s and key %s...", self.cert_path, self._key_path)
            check_call([
                'openssl', 'req', '-x509', '-days', '3650',
                '-newkey', 'rsa:2048', '-nodes', '-keyout', self._key_path,
                '-subj', '/C=US/O=NetworkOptix/CN=nxca',
                '-out', self.cert_path])
            log.info("New CA certificate %s can be added to browser or system as trusted.", self.cert_path)
        if not self._extensions_config_path.exists():
            log.debug("Save extensions config %s...", self._extensions_config_path)
            self._extensions_config_path.write_text(str.strip(dedent("""
                basicConstraints = CA:FALSE
                keyUsage = keyEncipherment
                subjectAltName = @alt_names
                [alt_names]
                DNS.1 = localhost
                IP.1 = 127.0.0.1
            """)))
        if not self._serial_path.exists():
            log.debug("Saving new serial file %s...", self._serial_path)
            self._serial_path.write_text('01')  # Could be any hex-encoded byte string up to 20 octets.
        self._gen_dir = ca_dir / 'gen'
        self._gen_dir.mkdir(parents=True, exist_ok=True)

    def generate_key_and_cert(self):
        log.debug("Generating key...")
        basename = self._gen_dir / self._serial_path.read_text().rstrip()
        key_path = basename.with_suffix('.key')
        check_call(['openssl', 'genrsa', '-out', str(key_path), '2048'])
        log.debug("Making request...")
        subject = '/C=US/O=NetworkOptix/CN=nxwitness'
        request_path = basename.with_suffix('.csr')
        check_call(['openssl', 'req', '-new', '-key', str(key_path), '-subj', subject, '-out', str(request_path)])
        log.debug("Signing request...")
        cert_path = basename.with_suffix('.crt')
        check_call([
            'openssl', 'x509', '-req',
            '-CA', str(self.cert_path), '-CAkey', str(self._key_path),
            '-CAserial', str(self._serial_path), '-CAcreateserial',
            '-in', str(request_path), '-out', str(cert_path),
            '-extfile', str(self._extensions_config_path)])
        return key_path.read_text() + cert_path.read_text()
