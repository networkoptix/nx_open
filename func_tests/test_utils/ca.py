import logging
import os

import subprocess
import textwrap

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
        if not os.path.exists(self.cert_path):
            check_call([
                'openssl', 'req', '-x509',
                '-newkey', 'rsa:2048', '-nodes', '-keyout', self._key_path,
                '-subj', '/C=US/O=NetworkOptix/CN=nxca',
                '-out', self.cert_path])
            with open(self._extensions_config_path, 'w') as extensions_config_file:
                extensions_config_file.write(str.strip(dedent("""
                    basicConstraints = CA:FALSE
                    keyUsage = keyEncipherment
                    subjectAltName = @alt_names
                    [alt_names]
                    DNS.1 = localhost
                    IP.1 = 127.0.0.1
                """)))

    def sign(self, request):
        log.debug("Sign request:\n%s", request)
        process = subprocess.Popen(
            ['openssl', 'x509', '-req',
             '-CA', self.cert_path, '-CAkey', self._key_path, '-CAserial', self._serial_path, '-CAcreateserial',
             '-extfile', self._extensions_config_path]
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        certificate, stderr = process.communicate(input=request)
        log.debug("Got certificate:\n%s", certificate)
        assert process.returncode == 0, "Issuing new cert ended with non-zero exit code; stderr:%r" % stderr
        return certificate
