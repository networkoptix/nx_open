import socket
from contextlib import closing

import pytest

from framework.port_check import port_is_open


@pytest.fixture(params=['tcp', 'udp'])
def sock_tuple(request, service_ports):
    localhost = '127.0.0.1'
    port = service_ports[98]
    if request.param == 'tcp':
        with closing(socket.socket()) as sock:
            sock.bind((localhost, port))
            sock.listen(2)
            yield sock, request.param, localhost, port
    if request.param == 'udp':
        with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as sock:
            sock.bind((localhost, port))
            yield sock, request.param, localhost, port


@pytest.fixture(params=['tcp', 'udp'])
def unbound_address_port(request, service_ports):
    return request.param, '127.0.0.1', service_ports[97]


def test_tcp_port_positively(sock_tuple):
    sock, protocol, address, port = sock_tuple
    assert port_is_open(protocol, address, port)


def test_tcp_port_negatively(unbound_address_port):
    protocol, address, port = unbound_address_port
    assert not port_is_open(protocol, address, port)
