from collections import namedtuple
from datetime import timedelta

import pytest
from netaddr import IPNetwork
from pathlib2 import Path


def pytest_addoption(parser):
    g = parser.getgroup('real camera test')
    g.addoption(
        '--rct-interface', default='enp3s0', help='Network interface with cameras')
    g.addoption(
        '--rct-network', default='192.168.200.111/24', type=IPNetwork,
        help='Network interface IP/mask')
    g.addoption(
        '--rct-expected-cameras', default='expected_cameras.yaml', type=_local_file,
        help='Stage rules for cameras')
    g.addoption(
        '--rct-camera-cycle-delay', default='1s', type=_time_delta,
        help='Delay between test cycles')
    g.addoption(
        '--rct-server-stage-delay', default='1m', type=_time_delta,
        help='Delay before last server stage')
    g.addoption(
        '--rct-stage-hard-timeout', default='1h', type=_time_delta,
        help='Limits stages delay')
    g.addoption(
        '--rct-camera-filter', default='*', type=lambda f: f.split(','),
        help='Comma separated list of camera names, posix wildcards are supported')


RctOptions = namedtuple(
    'RctOptions',
    [
        'interface',
        'network',
        'expected_cameras',
        'camera_cycle_delay',
        'server_stage_delay',
        'stage_hard_timeout',
        'camera_filter',
    ]
)


@pytest.fixture()
def rct_options(request):
    return RctOptions(**{
        name: request.config.getoption('--rct-' + name.replace('_', '-'))
        for name in RctOptions._fields})


def _local_file(value):
    return Path(__file__).parent / value


def _time_delta(value):
    if value.endswith('m'):
        return timedelta(minutes=int(value[:-1]))
    if value.endswith('h'):
        return timedelta(hours=int(value[:-1]))
    if value.endswith('s'):
        value = value[:-1]
    return timedelta(seconds=int(value))
