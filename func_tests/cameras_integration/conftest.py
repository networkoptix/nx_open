from collections import namedtuple
from datetime import timedelta

import pytest
from netaddr import IPNetwork
from pathlib2 import Path


def pytest_addoption(parser):
    g = parser.getgroup('real camera test')
    g.addoption('--rct-interface', default='enp3s0')
    g.addoption('--rct-network', default='192.168.200.111/24')
    g.addoption('--rct-expected-cameras', default='expected_cameras.yaml')
    g.addoption('--rct-camera-cycle-delay', default=_time_delta_str(seconds=1))
    g.addoption('--rct-server-stage-delay', default=_time_delta_str(minutes=1))
    g.addoption('--rct-stage-hard-timeout', default=_time_delta_str(hours=1))
    g.addoption('--rct-camera-filter', default='*')


@pytest.fixture()
def rct_options(request):
    options = dict(
        interface=request.config.getoption('--rct-interface'),
        network=IPNetwork(request.config.getoption('--rct-network')),
        expected_cameras=_file_path(request.config.getoption('--rct-expected-cameras')),
        camera_cycle_delay=_time_delta(request.config.getoption('--rct-camera-cycle-delay')),
        server_stage_delay=_time_delta(request.config.getoption('--rct-server-stage-delay')),
        stage_hard_timeout=_time_delta(request.config.getoption('--rct-stage-hard-timeout')),
        camera_filter=request.config.getoption('--rct-camera-filter').split(','),
    )

    Options = namedtuple('Options', options.keys())
    return Options(**options)


def _file_path(value):
    path = Path(value)
    if not path.is_absolute():
        path = Path(__file__).parent / path
    return path


def _time_delta(value):
    # TODO: Add support for 'm' and 'h' suffixes.
    return timedelta(seconds=int(value))


def _time_delta_str(**values):
    return str(int(timedelta(**values).total_seconds()))