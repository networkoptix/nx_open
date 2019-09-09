import ssl
import socket
import OpenSSL
from datetime import datetime


def get_certificate(host, port=443, timeout=10):
    context = ssl.create_default_context()
    conn = socket.create_connection((host, port))
    sock = context.wrap_socket(conn, server_hostname=host)
    sock.settimeout(timeout)

    try:
        der_cert = sock.getpeercert(True)
    finally:
        sock.close()

    return ssl.DER_cert_to_PEM_cert(der_cert)


def cert_valid_days(host):
    """:returns number of days host certificate is valid since now"""
    certificate = get_certificate(host)
    x509 = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM, certificate)
    not_after = datetime.strptime(x509.get_notAfter().decode(), '%Y%m%d%H%M%SZ')

    return (not_after - datetime.now()).days
