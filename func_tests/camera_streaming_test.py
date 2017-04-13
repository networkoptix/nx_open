import logging
import time
import json
import pytest

log = logging.getLogger(__name__)


EXPECTED_TRANSPORT_LIST = set(['rtsp', 'hls', 'mjpeg', 'webm'])
HISTORY_WAIT_TIMEOUT_SEC = 60


@pytest.fixture
def env(env_builder, server):
    one = server()
    two = server()
    return env_builder(merge_servers=[one, two], one=one, two=two)


def wait_for_and_check_camera_history(camera, server_list, expected_servers_order):
    t = time.time()
    camera_history_responses = []
    while len(camera_history_responses) < len(server_list):
        for server in server_list:
            response = server.rest_api.ec2.cameraHistory.GET(
                cameraId=camera.id, startTime=0, endTime='now')
            assert len(response) == 1, repr(response)  # must contain exactly one record for one camera
            servers_order = [item['serverGuid'] for item in response[0]['items']]
            log.debug('Received camera history servers order: %s', servers_order)
            if servers_order == [server.ecs_guid for server in expected_servers_order]:
                camera_history_responses.append(response)
                continue
            if time.time() - t > HISTORY_WAIT_TIMEOUT_SEC:
                pytest.fail('Timed out while waiting for proper camera history (%s seconds)' % HISTORY_WAIT_TIMEOUT_SEC)
        time.sleep(5)
    assert len(camera_history_responses) == len(server_list)
    for response in camera_history_responses[1:]:
        assert response == camera_history_responses[0]  # All responses must be the same
    ts_seq = [item['timestampMs'] for item in camera_history_responses[0][0]['items']]
    assert ts_seq == sorted(ts_seq)  # timestamps must be in sorted order

# https://networkoptix.atlassian.net/browse/TEST-181
# transport check part (3):
def check_media_stream_transports(server):
    camera_info_list = server.rest_api.ec2.getCamerasEx.GET()
    assert camera_info_list  # At least one camera must be returned for following check to work
    for camera_info in camera_info_list:
        for add_params_rec in camera_info['addParams']:
            if add_params_rec['name'] == 'mediaStreams':
                value = json.loads(add_params_rec['value'])
                transports = set()
                for codec_rec in value['streams']:
                    transports |= set(codec_rec['transports'])
                log.info('Server %s returned following transports for camera %s: %s',
                         server, camera_info['physicalId'], ', '.join(sorted(transports)))
                assert transports == EXPECTED_TRANSPORT_LIST, repr(transports)
                break
        else:
            pytest.fail('ec2/getCamerasEx addParams element does not have "mediaStreams" record')

# https://networkoptix.atlassian.net/browse/TEST-178
# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/77234376/Camera+history+test
@pytest.mark.testcam
def test_camera_switching_should_be_represented_in_history(env, camera):
    camera.start_streaming()
    camera.wait_until_discovered_by_server([env.one, env.two])
    camera.switch_to_server(env.one)
    env.one.start_recording_camera(camera)
    wait_for_and_check_camera_history(camera, [env.one, env.two], [env.one])
    camera.switch_to_server(env.two)
    wait_for_and_check_camera_history(camera, [env.one, env.two], [env.one, env.two])
    camera.switch_to_server(env.one)
    wait_for_and_check_camera_history(camera, [env.one, env.two], [env.one, env.two, env.one])
    env.one.stop_recording_camera(camera)

    # https://networkoptix.atlassian.net/browse/VMS-4180
    stream_type = 'hls'
    stream = env.one.get_media_stream(stream_type, camera)
    metadata_list = stream.load_archive_stream_metadata(
        '%s-stream-media-%s' % (env.artifact_path_prefix, stream_type), pos=0, duration=3000)
    assert metadata_list  # Must not be empty

    check_media_stream_transports(env.one)
