import datetime
import json
import logging
import time

import pytest

from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)


EXPECTED_TRANSPORT_LIST = {'rtsp', 'hls', 'mjpeg', 'webm'}
HISTORY_WAIT_TIMEOUT_SEC = 2*60
CAMERA_DISCOVERY_WAIT_TIMEOUT = datetime.timedelta(seconds=60)


def switch_to_server(camera_id, server):
    server_guid = server.api.get_server_id()
    server.api.generic.post('ec2/saveCamera', dict(id=camera_id, parentId=server_guid))
    d = None
    for d in server.api.generic.get('ec2/getCamerasEx'):
        if d['id'] == camera_id:
            break
    if d is None:
        pytest.fail('Camera %s is unknown for server %s' % (camera_id, server))
    assert d['parentId'] == server_guid


def wait_for_and_check_camera_history(camera_id, server_list, expected_servers_order):
    t = time.time()
    while True:
        camera_history_responses = []
        for server in server_list:
            response = server.api.generic.get('ec2/cameraHistory', dict(
                cameraId=camera_id, startTime=0, endTime='now'))
            assert len(response) == 1, repr(response)  # must contain exactly one record for one camera_id
            servers_order = [item['serverGuid'] for item in response[0]['items']]
            _logger.debug('Received camera_id history servers order: %s', servers_order)
            if servers_order == [server.api.get_server_id() for server in expected_servers_order]:
                camera_history_responses.append(response)
                continue
            if time.time() - t > HISTORY_WAIT_TIMEOUT_SEC:
                pytest.fail('Timed out while waiting for proper camera_id history (%s seconds)' % HISTORY_WAIT_TIMEOUT_SEC)
        if len(camera_history_responses) >= len(server_list):
            break
        time.sleep(5)
    assert len(camera_history_responses) == len(server_list)
    for response in camera_history_responses[1:]:
        assert response == camera_history_responses[0]  # All responses must be the same
    ts_seq = [item['timestampMs'] for item in camera_history_responses[0][0]['items']]
    assert ts_seq == sorted(ts_seq)  # timestamps must be in sorted order


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
    one, two = two_merged_mediaservers

    camera_id = wait_for_true(
        lambda: one.api.find_camera(camera.mac_addr) or two.api.find_camera(camera.mac_addr),
        description="Test Camera is discovered",
        timeout_sec=300)
    switch_to_server(camera_id, one)
    with one.api.camera_recording(camera_id):
        wait_for_and_check_camera_history(camera_id, [one, two], [one])
        switch_to_server(camera_id, two)
        wait_for_and_check_camera_history(camera_id, [one, two], [one, two])
        switch_to_server(camera_id, one)
        wait_for_and_check_camera_history(camera_id, [one, two], [one, two, one])

    # https://networkoptix.atlassian.net/browse/VMS-4180
    stream_type = 'hls'
    stream = one.api.get_media_stream(stream_type, camera.mac_addr)
    metadata_list = stream.load_archive_stream_metadata(
        artifact_factory(['stream-media', stream_type]), pos=0, duration=3000)
    assert metadata_list  # Must not be empty

    check_media_stream_transports(one)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()
