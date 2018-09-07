import pytest
from netaddr import IPAddress

from framework.mediaserver_api import DEFAULT_API_PASSWORD, DEFAULT_API_USER, MediaserverApi


def pytest_addoption(parser):
    parser.addoption(
        '--api', '-A',
        metavar='[USER:PASSWORD@][ADDRESS[:PORT]]',
        default='{}:{}@'.format(DEFAULT_API_USER, DEFAULT_API_PASSWORD),
        help="Use live mediaserver for test that require Mediaserver API only [%(default)s]")


@pytest.fixture()
def runner_address():
    return IPAddress('127.0.0.1')


@pytest.fixture()
def one_mediaserver_api(pytestconfig):
    return MediaserverApi(pytestconfig.getoption('--api'))
