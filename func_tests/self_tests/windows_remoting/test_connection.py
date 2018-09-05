import logging
import socket
import struct
from contextlib import closing
from pprint import pformat

import requests

from framework.waiting import retry_on_exception

_logger = logging.getLogger(__name__)


def test_port_open(windows_vm):
    """Port is open even if OS is still booting."""
    hostname = windows_vm.port_map.remote.address
    port = windows_vm.port_map.remote.tcp(5985)
    client = socket.socket()
    # Forcefully reset connection when closed.
    # Otherwise, there are FIN-ACK and ACK from other connection before normal handshake in Wireshark.
    # See: https://stackoverflow.com/a/6440364/1833960
    l_onoff = 1
    l_linger = 0
    client.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', l_onoff, l_linger))
    with closing(client):
        retry_on_exception(
            lambda: client.connect((str(hostname), port)),
            socket.timeout,
            'connection to {}:{} can be established'.format(hostname, port),
            timeout_sec=300)


def test_http_is_understood(windows_vm):
    hostname = windows_vm.port_map.remote.address
    port = windows_vm.port_map.remote.tcp(5985)
    url = 'http://{}:{}/wsman'.format(hostname, port)
    response = retry_on_exception(
        lambda: requests.get(url),
        requests.ConnectionError,
        "HTTP response is received",
        timeout_sec=600)
    _logger.debug(
        "Response:\n%d\n%s\n%r",
        response.status_code,
        pformat(response.headers),
        response.content)
    assert 100 <= response.status_code < 600
