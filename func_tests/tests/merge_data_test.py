import logging
from datetime import datetime

import datadiff
import pytest
import pytz

from framework.mediaserver_api import MediaserverApiRequestError, TimePeriod, Unauthorized
from framework.waiting import Wait

_logger = logging.getLogger(__name__)


@pytest.mark.parametrize(('layout_file', 'target_alias', 'proxy_alias'), [
    ('unrouted-merge_toward_proxy-request_proxy.yaml', 'first', 'second'),
    ('unrouted-merge_toward_proxy-request_sides.yaml', 'first', 'second'),
    ('unrouted-merge_toward_sides-request_sides.yaml', 'first', 'second'),
    ('direct-merge_toward_requested.yaml', 'first', 'second'),
    ('direct-merge_toward_requested.yaml', 'second', 'first'),
    ('nat-merge_toward_inner.yaml', 'inner', 'outer'),
    ('nat-merge_toward_inner.yaml', 'outer', 'inner'),
    ])
def test_responses_are_equal(system, target_alias, proxy_alias):
    wait = Wait("until responses become equal")
    target_guid = system[target_alias].api.get_server_id()
    api_endpoints = [
        'api/moduleInformation',
        'ec2/getMediaServersEx',
        'ec2/testConnection',
        'ec2/getStorages',
        'ec2/getResourceParams',
        'ec2/getCamerasEx',
        'ec2/getUsers',
        ]
    for api_endpoint in api_endpoints:
        while True:
            try:
                response_via_proxy = system[proxy_alias].api.generic.get(
                    api_endpoint,
                    headers={'X-server-guid': target_guid})
            except (MediaserverApiRequestError, Unauthorized) as exc:
                # We can get MediaserverApiRequestError or Unauthorized here,
                # if mediaservers doesn't sync some data (interfaces, users, etc) yet.
                if not wait.again():
                    assert False, ("Can't send '{}' request via proxy: {}".format(
                        api_endpoint, str(exc)))
                else:
                    wait.sleep()
                    continue
            response_direct = system[target_alias].api.generic.get(api_endpoint)
            diff = datadiff.diff(
                response_via_proxy, response_direct,
                fromfile='via proxy', tofile='direct',
                context=100)
            if not diff:
                break
            if not wait.again():
                assert not diff, 'Found difference:\n{}'.format(diff)
            wait.sleep()

    assert not system[target_alias].installation.list_core_dumps()
    assert not system[proxy_alias].installation.list_core_dumps()


def assert_server_stream(server, camera, sample_media_file, stream_type, artifacts_dir, start_time):
    assert TimePeriod(start_time, sample_media_file.duration) in server.api.get_recorded_time_periods(camera.id)
    stream = server.api.get_media_stream(stream_type, camera.mac_addr)
    local_dir = artifacts_dir / 'stream-media-{}'.format(stream_type)
    local_dir.mkdir(parents=True, exist_ok=True)
    metadata_list = stream.load_archive_stream_metadata(
        local_dir,
        pos=start_time, duration=sample_media_file.duration)
    for metadata in metadata_list:
        assert metadata.width == sample_media_file.width
        assert metadata.height == sample_media_file.height


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/23920653/Connection+behind+NAT#ConnectionbehindNAT-test_get_streams
@pytest.mark.slow
@pytest.mark.parametrize('layout_file', ['nat-merge_toward_inner.yaml'])
def test_get_streams(artifacts_dir, system, camera, sample_media_file, stream_type):
    system['inner'].api.add_camera(camera)
    start_time_1 = datetime(2017, 1, 27, tzinfo=pytz.utc)
    system['outer'].storage.save_media_sample(camera, start_time_1, sample_media_file)
    system['outer'].api.rebuild_archive()
    start_time_2 = datetime(2017, 2, 27, tzinfo=pytz.utc)
    system['inner'].storage.save_media_sample(camera, start_time_2, sample_media_file)
    system['inner'].api.rebuild_archive()
    assert_server_stream(
        system['inner'], camera, sample_media_file, stream_type, artifacts_dir, start_time_1)
    assert_server_stream(
        system['outer'], camera, sample_media_file, stream_type, artifacts_dir, start_time_1)
    assert_server_stream(
        system['inner'], camera, sample_media_file, stream_type, artifacts_dir, start_time_2)
    assert_server_stream(
        system['outer'], camera, sample_media_file, stream_type, artifacts_dir, start_time_2)

    assert not system['outer'].installation.list_core_dumps()
    assert not system['inner'].installation.list_core_dumps()
