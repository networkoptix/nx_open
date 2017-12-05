import logging
import os

from subprocess import Popen, check_output, check_call, PIPE
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

    def generate_key_and_cert(self):
        log.debug("Generate key.")
        key = check_output(['openssl', 'genrsa', '2048'])
        request_subject = '/C=US/O=NetworkOptix/CN=nxwitness'
        request_command_line = ['openssl', 'req', '-new', '-key', '-', '-subj', request_subject]
        log.debug("Make request.")
        request_process = Popen(request_command_line, stdin=PIPE, stdout=PIPE, stderr=PIPE)
        request, request_stderr = request_process.communicate(key)
        assert request_process.returncode == 0, "Non-zero exit code when making request:\n%s" % request_stderr
        log.debug("Sign request:\n%s", request)
        cert_command_line = [
            'openssl', 'x509', '-req',
            '-CA', self.cert_path, '-CAkey', self._key_path, '-CAserial', self._serial_path, '-CAcreateserial',
            '-extfile', self._extensions_config_path]
        cert_process = Popen(cert_command_line, stdin=PIPE, stdout=PIPE, stderr=PIPE)
        cert, cert_stderr = cert_process.communicate(input=request)
        log.debug("Got certificate:\n%s", cert)
        assert cert_process.returncode == 0, "Non-zero exit code when signing request:\n%s" % cert_stderr
        return key + cert
