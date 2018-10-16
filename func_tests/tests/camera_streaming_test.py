import datetime
import json
import logging

import pytest

from framework.installation.mediaserver import Mediaserver
from framework.waiting import wait_for_equal, wait_for_truthy

_logger = logging.getLogger(__name__)


EXPECTED_TRANSPORT_LIST = {'rtsp', 'hls', 'mjpeg', 'webm'}
HISTORY_WAIT_TIMEOUT_SEC = 2*60
CAMERA_DISCOVERY_WAIT_TIMEOUT = datetime.timedelta(seconds=60)


# https://networkoptix.atlassian.net/browse/TEST-181
# transport check part (3):
def check_media_stream_transports(server):
    camera_info_list = server.api.generic.get('ec2/getCamerasEx')
    assert camera_info_list  # At least one camera must be returned for following check to work
    for camera_info in camera_info_list:
        for add_params_rec in camera_info['addParams']:
            if add_params_rec['name'] == 'mediaStreams':
                value = json.loads(add_params_rec['value'])
                transports = set()
                for codec_rec in value['streams']:
                    transports |= set(codec_rec['transports'])
                _logger.info(
                    'Mediaserver %s returned following transports for camera %s: %s',
                    server, camera_info['physicalId'], ', '.join(sorted(transports)))
                assert transports == EXPECTED_TRANSPORT_LIST, repr(transports)
                break
        else:
            pytest.fail('ec2/getCamerasEx addParams element does not have "mediaStreams" record')


# https://networkoptix.atlassian.net/browse/TEST-178
# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/77234376/Camera+history+test
@pytest.mark.testcam
def test_camera_switching_should_be_represented_in_history(artifact_factory, two_merged_mediaservers, camera_pool, camera):
    for server in two_merged_mediaservers:
        server.installation.ini_config('test_camera').set('discoveryPort', str(camera_pool.discovery_port))
    one, two = two_merged_mediaservers  # type: (Mediaserver, Mediaserver)

    camera_id = wait_for_truthy(
        lambda: one.api.find_camera(camera.mac_addr) or two.api.find_camera(camera.mac_addr),
        description="Test Camera is discovered",
        timeout_sec=300)
    one.api.take_camera(camera_id)
    _logger.info("Start recording to make camera appear in camera history.")
    with one.api.camera_recording(camera_id):
        # After camera is moved, synchronization shouldn't take long.
        # History update takes a while. History synchronization does not.
        wait_for_equal(one.api.camera(camera_id).server, one.api.get_server_id(), timeout_sec=30)
        wait_for_equal(two.api.camera(camera_id).server, one.api.get_server_id(), timeout_sec=5)
        history = [one.api.get_server_id()]
        wait_for_equal(one.api.camera(camera_id).history, history, timeout_sec=30)
        wait_for_equal(two.api.camera(camera_id).history, history, timeout_sec=5)
        _logger.info("Switch camera to `two`.")
        two.api.take_camera(camera_id)
        wait_for_equal(one.api.camera(camera_id).server, two.api.get_server_id(), timeout_sec=30)
        wait_for_equal(two.api.camera(camera_id).server, two.api.get_server_id(), timeout_sec=5)
        history.append(two.api.get_server_id())
        wait_for_equal(one.api.camera(camera_id).history, history, timeout_sec=30)
        wait_for_equal(two.api.camera(camera_id).history, history, timeout_sec=5)
        _logger.info("Switch camera back to `one`.")
        one.api.take_camera(camera_id)
        wait_for_equal(one.api.camera(camera_id).server, one.api.get_server_id(), timeout_sec=30)
        wait_for_equal(two.api.camera(camera_id).server, one.api.get_server_id(), timeout_sec=5)
        history.append(one.api.get_server_id())
        wait_for_equal(one.api.camera(camera_id).history, history, timeout_sec=60)
        wait_for_equal(two.api.camera(camera_id).history, history, timeout_sec=5)

    # https://networkoptix.atlassian.net/browse/VMS-4180
    stream_type = 'hls'
    stream = one.api.get_media_stream(stream_type, camera.mac_addr)
    metadata_list = stream.load_archive_stream_metadata(
        artifact_factory(['stream-media', stream_type]), pos=0, duration=3000)
    assert metadata_list  # Must not be empty

    check_media_stream_transports(one)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()
