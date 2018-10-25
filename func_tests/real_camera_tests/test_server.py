import json
import logging

import pytest
import oyaml as yaml

from framework.mediaserver_api import MediaserverApiRequestError
from . import execution

_logger = logging.getLogger(__name__)


@pytest.fixture(scope='session')
def one_vm_type():
    return 'linux'


def test_cameras(rct_options, one_vm, one_licensed_mediaserver, artifacts_dir):
    one_licensed_mediaserver.os_access.networking.setup_network(
        one_vm.hardware.plug_bridged(rct_options.interface), rct_options.network)

    def save_result(name, data):
        file_path = artifacts_dir / name
        file_path.with_suffix('.json').write_bytes(json.dumps(data, indent=4))
        file_path.with_suffix('.yaml').write_bytes(
            yaml.safe_dump(data, default_flow_style=False, width=1000))
        _logger.info('Save result: %s', file_path)

    stand = execution.Stand(
        one_licensed_mediaserver,
        yaml.load(rct_options.expected_cameras.read_bytes()) or {},
        stage_hard_timeout=rct_options.stage_hard_timeout,
        camera_filters=rct_options.camera_filter)
    try:
        stand.run_all_stages(rct_options.camera_cycle_delay, rct_options.server_stage_delay)
    finally:
        save_result('module_information', stand.server_information)
        #save_result('test_results', stand.report)
        for _ in range(10):
            try:
                save_result('all_cameras', stand.all_cameras(verbose=True))
            except MediaserverApiRequestError:
                pass
            else:
                break

    assert stand.is_successful
