import pytest
import yaml
from pathlib2 import Path

import execution


@pytest.fixture(scope='session')
def one_vm_type():
    return 'linux'


@pytest.fixture
def config(test_config):
    return test_config.with_defaults(
        CAMERAS_INTERFACE='enp5s0',
        CAMERAS_NETWORK='192.168.0',
        CAMERAS_NETWORK_IP='200',
        EXPECTED_CAMERAS_FILE=Path(__file__).parent / 'expected_cameras.yaml',
        CYCLE_DELAY_S=1,
        )


def test_cameras(one_vm, one_licensed_server, config, work_dir):
    one_licensed_server.os_access.networking.setup_network(
        one_vm.hardware.plug_bridged(config.CAMERAS_INTERFACE),
        config.CAMERAS_NETWORK, config.CAMERAS_NETWORK_IP)

    def save_yaml(data, file_name):
        serialized = yaml.safe_dump(data, default_flow_style=False, width=1000)
        (work_dir / (file_name + '.yaml')).write_bytes(serialized)

    stand = execution.Stand(
        one_licensed_server,
        yaml.load(Path(config.EXPECTED_CAMERAS_FILE).read_bytes()))

    try:
        stand.run_all_stages(config.CYCLE_DELAY_S)

    finally:
        save_yaml(one_licensed_server.api.get('api/moduleInformation'), 'module_information')
        save_yaml(one_licensed_server.get_resources('CamerasEx'), 'all_cameras')
        save_yaml(stand.report(), 'test_report')

    assert stand.is_successful
