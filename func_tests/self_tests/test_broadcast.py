import errno
import socket
from contextlib import closing

import pytest

from framework.os_access.exceptions import Timeout


@pytest.fixture()
def udp_socket():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    with closing(s):
        yield s


@pytest.fixture()
def port(service_ports, udp_socket):
    for port in service_ports[15:20]:
        try:
            udp_socket.bind(('0.0.0.0', port))
        except socket.error as e:
            if e != errno.EADDRINUSE:
                raise
            continue
        return port
    raise EnvironmentError("Cannot find vacant port")


pytest_plugins = ['fixtures.ad_hoc_ssh']


@pytest.mark.xfail(reason="`nc` on Ubuntu 14.04 doesn't support broadcasts")
def test_broadcast(ssh, udp_socket, port):
    sent_data = b"Hi there!"
    try:
        ssh.run_command(['nc', '-vv', '-ub', '255.255.255.255', port], input=sent_data, timeout_sec=1)
    except Timeout:
        pass
    received_data, address_info = udp_socket.recvfrom(4096)
    assert received_data == sent_data
