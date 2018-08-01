import json
from datetime import timedelta

import pytest
import yaml
from pathlib2 import Path

from . import execution


@pytest.fixture(scope='session')
def one_vm_type():
    return 'linux'


@pytest.fixture
def config(test_config):
    return test_config.with_defaults(
        CAMERAS_INTERFACE='enp3s0',
        CAMERAS_NETWORK='192.168.200.111/24',
        EXPECTED_CAMERAS_FILE=Path(__file__).parent / 'expected_cameras.yaml',
        CAMERA_CYCLE_DELAY='00:00:01',
        SERVER_STAGE_DELAY='00:01:00',
        STAGE_HARD_TIMEOUT='01:00:00',
        )


def parse_timedelta(value):
    parts = list(reversed(str(value).split(':')))
    if len(parts) > 3:
        raise ValueError('{} is not a valid timedelta'.format(repr(value)))

    def number(index):
        return int(parts[index]) if len(parts) > index else 0

    return timedelta(seconds=number(0), minutes=number(1), hours=number(2))


def test_cameras(one_vm, one_licensed_mediaserver, config, artifacts_dir):
    one_licensed_mediaserver.os_access.networking.setup_network(
        one_vm.hardware.plug_bridged(config.CAMERAS_INTERFACE), config.CAMERAS_NETWORK)

    def save_result(name, data):
        file_path = artifacts_dir / name
        file_path.with_suffix('.json').write_bytes(json.dumps(data, indent=4, sort_keys=True))
        file_path.with_suffix('.yaml').write_bytes(
            yaml.safe_dump(data, default_flow_style=False, width=1000))

    expected_cameras = Path(config.EXPECTED_CAMERAS_FILE)
    if not expected_cameras.is_absolute():
        expected_cameras = Path(__file__).parent / expected_cameras

    stand = execution.Stand(
        one_licensed_mediaserver,
        yaml.load(expected_cameras.read_bytes()),
        parse_timedelta(config.STAGE_HARD_TIMEOUT))
    try:
        stand.run_all_stages(parse_timedelta(config.CAMERA_CYCLE_DELAY),
                             parse_timedelta(config.SERVER_STAGE_DELAY))
    finally:
        save_result('module_information', stand.server_information)
        save_result('all_cameras', stand.all_cameras())
        save_result('test_results', stand.report)

    assert stand.is_successful
