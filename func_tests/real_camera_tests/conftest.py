from collections import namedtuple
from datetime import timedelta

import pytest
from netaddr import IPNetwork
from pathlib2 import Path

from defaults import defaults


def pytest_addoption(parser):
    g = parser.getgroup('real camera test')
    g.addoption(
        '--rct-interface', default=defaults.get('rct_interface'), help='Network interface with cameras')
    g.addoption(
        '--rct-network', default=defaults.get('rct_network'), type=IPNetwork,
        help='Network interface IP/mask')
    g.addoption(
        '--rct-expected-cameras', default=defaults.get('rct_expected_cameras'), type=Path(__file__).with_name,
        help='Stage rules for cameras')
    g.addoption(
        '--rct-camera-cycle-delay', default=defaults.get('rct_camera_cycle_delay', '1s'), type=_time_delta,
        help='Delay between test cycles')
    g.addoption(
        '--rct-server-stage-delay', default=defaults.get('rct_server_stage_delay', '1m'), type=_time_delta,
        help='Delay before last server stage')
    g.addoption(
        '--rct-stage-hard-timeout', default=defaults.get('rct_stage_hard_timeout', '1h'), type=_time_delta,
        help='Limits stages delay')
    g.addoption(
        '--rct-camera-filter', default=defaults.get('rct_camera_filter', '*'), type=lambda f: f.split(','),
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


def _time_delta(value):
    if value.endswith('m'):
        return timedelta(minutes=int(value[:-1]))
    if value.endswith('h'):
        return timedelta(hours=int(value[:-1]))
    if value.endswith('s'):
        value = value[:-1]
    return timedelta(seconds=int(value))
