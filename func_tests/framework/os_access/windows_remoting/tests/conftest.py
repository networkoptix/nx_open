import pytest
import winrm


@pytest.fixture(name='address')
def get_address():
    return '127.0.0.1'
    return '10.5.0.13'


@pytest.fixture(name='port')
def get_port():
    return 10005
    return 5985


@pytest.fixture(name='protocol')
def make_protocol(address, port, username='Administrator', password='qweasd123'):
    return winrm.Protocol(
        'http://{address}:{port}/wsman'.format(address=address, port=port),
        username=username, password=password,
        transport='ntlm')
