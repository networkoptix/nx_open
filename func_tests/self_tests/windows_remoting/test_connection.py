import logging
import socket
import struct
from contextlib import closing
from pprint import pformat

import requests

from framework.utils import Wait

_logger = logging.getLogger(__name__)


def test_port_open(windows_vm_info):
    """Port is open even if OS is still booting."""
    hostname, port = windows_vm_info.ports['tcp', 5985]
    client = socket.socket()
    # Forcefully reset connection when closed.
    # Otherwise, there are FIN-ACK and ACK from other connection before normal handshake in Wireshark.
    # See: https://stackoverflow.com/a/6440364/1833960
    l_onoff = 1
    l_linger = 0
    client.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', l_onoff, l_linger))
    with closing(client):
        wait = Wait('for connection', timeout_sec=300)
        while True:
            try:
                client.connect((hostname, port))
            except socket.timeout:
                if not wait.sleep_and_continue():
                    assert False, "Cannot connect to VM"
            else:
                break


def test_http_is_understood(windows_vm_info):
    hostname, port = windows_vm_info.ports['tcp', 5985]
    wait = Wait('for HTTP response', timeout_sec=600)
    while True:
        try:
            response = requests.get('http://{}:{}/wsman'.format(hostname, port))
        except requests.ConnectionError:
            if not wait.sleep_and_continue():
                assert False, "Not HTTP response yet"
        else:
            _logger.debug(
                "Response:\n%d\n%s\n%r",
                response.status_code,
                pformat(response.headers),
                response.content)
            assert 100 <= response.status_code < 600
            break
