import logging
import socket
import struct
from contextlib import closing
from pprint import pformat

import pytest
import requests

from framework.os_access.windows_remoting.winrm_access import WinRMAccess
from framework.utils import Wait
from framework.vms.hypervisor import obtain_running_vm

_logger = logging.getLogger(__name__)


@pytest.fixture()
def vm(vm_factory):
    vm = vm_factory.allocate('single-windows-vm', vm_type='windows')
    yield vm
    vm_factory.release(vm)


@pytest.fixture()
def vm_info(configuration, hypervisor, vm_registries):
    windows_vm_registry = vm_registries['windows']
    vm_index, vm_name = windows_vm_registry.take('raw-windows')
    vm_info = obtain_running_vm(hypervisor, vm_name, vm_index, configuration['vm_types'])
    yield vm_info
    windows_vm_registry.free(vm_name)


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
