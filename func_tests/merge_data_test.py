import logging
from datetime import datetime

import datadiff
import pytest
import pytz
import requests
import requests.auth

from test_utils.server import TimePeriod
from test_utils.utils import Wait

log = logging.getLogger(__name__)


@pytest.mark.quick
@pytest.mark.parametrize(('layout_file', 'target_alias', 'proxy_alias'), [
    # ('unrouted-merge_toward_proxy-request_proxy.yaml', 'first', 'second'),
    # ('unrouted-merge_toward_proxy-request_sides.yaml', 'first', 'second'),
    ('unrouted-merge_toward_sides-request_sides.yaml', 'first', 'second'),
    # ('direct.yaml', 'first', 'second'),
    # ('direct.yaml', 'second', 'first'),
    # ('nat-merge_toward_inner.yaml', 'inner', 'outer'),
    # ('nat-merge_toward_inner.yaml', 'outer', 'inner'),
    ])
@pytest.mark.parametrize('api_endpoint', [
    'ec2/getMediaServersEx',
    # 'api/moduleInformation',
    # 'ec2/testConnection',
    # 'ec2/getStorages',
    # 'ec2/getResourceParams',
    # 'ec2/getCamerasEx',
    # 'ec2/getUsers',
    ])
def test_responses_are_equal(network, target_alias, proxy_alias, api_endpoint):
    wait = Wait("until responses become equal")
    while True:
        response_direct = requests.get(
            network[target_alias].rest_api_url + api_endpoint,
            auth=requests.auth.HTTPDigestAuth(network[target_alias].user, network[target_alias].password))
        response_via_proxy = requests.get(
            network[proxy_alias].rest_api_url + api_endpoint,
            auth=requests.auth.HTTPDigestAuth(network[proxy_alias].user, network[proxy_alias].password),
            headers={'X-server-guid': network[target_alias].ecs_guid})
        diff = datadiff.diff(
            response_via_proxy.json(), response_direct.json(),
            fromfile='via proxy', tofile='direct',
            context=100)
        if not diff:
            break
        if not wait.sleep_and_continue():
            assert not diff, 'Found difference:\n{}'.format(diff)


def assert_server_stream(server, camera, sample_media_file, stream_type, artifact_factory, start_time):
    assert TimePeriod(start_time, sample_media_file.duration) in server.get_recorded_time_periods(camera)
    stream = server.get_media_stream(stream_type, camera)
    metadata_list = stream.load_archive_stream_metadata(
        artifact_factory(['stream-media', stream_type]),
        pos=start_time, duration=sample_media_file.duration)
    for metadata in metadata_list:
        assert metadata.width == sample_media_file.width
        assert metadata.height == sample_media_file.height


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/23920653/Connection+behind+NAT#ConnectionbehindNAT-test_get_streams
@pytest.mark.slow
@pytest.mark.parametrize('layout_file', ['nat-merge_toward_inner.yaml'])
def test_get_streams(artifact_factory, servers, camera, sample_media_file, stream_type):
    servers['second'].add_camera(camera)
    start_time_1 = datetime(2017, 1, 27, tzinfo=pytz.utc)
    servers['first'].storage.save_media_sample(camera, start_time_1, sample_media_file)
    servers['first'].rebuild_archive()
    start_time_2 = datetime(2017, 2, 27, tzinfo=pytz.utc)
    servers['second'].storage.save_media_sample(camera, start_time_2, sample_media_file)
    servers['second'].rebuild_archive()
    assert_server_stream(
        servers['second'], camera, sample_media_file, stream_type, artifact_factory, start_time_1)
    assert_server_stream(
        servers['first'], camera, sample_media_file, stream_type, artifact_factory, start_time_1)
    assert_server_stream(
        servers['second'], camera, sample_media_file, stream_type, artifact_factory, start_time_2)
    assert_server_stream(
        servers['first'], camera, sample_media_file, stream_type, artifact_factory, start_time_2)
