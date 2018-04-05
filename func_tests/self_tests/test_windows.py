import logging
from contextlib import closing
import socket
from pprint import pformat

import pytest
import requests
import struct

from framework.os_access.windows_remoting.winrm_access import WinRMAccess
from framework.utils import Wait

_logger = logging.getLogger(__name__)


@pytest.fixture()
def vm(vm_pools):
    return vm_pools['windows'].get('single')


@pytest.fixture(scope='session')
def vm_factory(vm_factories):
    return vm_factories['windows']


@pytest.fixture(scope='session')
def vm_registry(vm_registries):
    return vm_registries['windows']


@pytest.fixture()
def vm_info(vm_registry, vm_factory):
    index = vm_registry.reserve('windows-test-connection')
    info = vm_factory.find_or_clone(index)
    yield info
    vm_registry.relinquish(index)


def test_connection(vm_info):
    """Port is open even if OS is still booting."""
    hostname, port = vm_info.ports['tcp', 5985]
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


def test_http(vm_info):
    hostname, port = vm_info.ports['tcp', 5985]
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


def test_winrm_works(vm_info):
    hostname, port = vm_info.ports['tcp', 5985]
    access = WinRMAccess(hostname, port)
    assert access.is_working()


def test_api_ping(vm):
    vm.api.get('api/ping')
