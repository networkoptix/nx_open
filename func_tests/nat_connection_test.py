import logging
import time
from datetime import datetime

import pytest
import pytz
import requests
import requests.auth

from test_utils.server import MEDIASERVER_MERGE_TIMEOUT
from test_utils.server import TimePeriod
from test_utils.utils import SimpleNamespace, datetime_utc_now

log = logging.getLogger(__name__)


@pytest.fixture(params=['direct', 'nat'])
def nat_schema(request):
    return request.param


@pytest.fixture
def env(box, server_factory, http_schema, nat_schema):
    if nat_schema == 'direct':
        env_factory = direct_env
    elif nat_schema == 'nat':
        env_factory = nat_env
    else:
        assert False, "nat_schema must be either 'direct' or 'nat'"
    return env_factory(box, server_factory, http_schema)

def direct_env(box, server_factory, http_schema):
    in_front = server_factory('in_front', http_schema=http_schema)
    behind = server_factory('behind', http_schema=http_schema)
    behind.merge([in_front])
    return SimpleNamespace(in_front=in_front, behind=behind)

def nat_env(box, server_factory, http_schema):
    in_front_net    = '10.10.1/24'
    in_front_net_gw = '10.10.1.1/24'
    behind_net      = '10.10.2/24'
    behind_net_gw   = '10.10.2.1/24'
    router = box('router', ip_address_list=[in_front_net_gw, behind_net_gw],
                 install_server=False, provision_scripts=['box-provision-nat-router.sh'])
    behind_box = box('behind', ip_address_list=[behind_net], provision_scripts=['box-provision-nat-behind.sh'])
    in_front_box = box('in-front', ip_address_list=[in_front_net])
    in_front = server_factory('in_front', box=in_front_box, http_schema=http_schema)
    behind = server_factory('behind', box=behind_box, http_schema=http_schema)
    # behind must initiate merge, it can reach sever in_front, but not vise-versa
    behind.merge([in_front])
    return SimpleNamespace(in_front=in_front, behind=behind)


def wait_for_servers_return_same_results_to_api_call(env, method, api_object, api_method):
    log.info('TEST for %s %s.%s:', method, api_object, api_method)
    start_time = datetime_utc_now()
    while True:
        result_in_front = env.in_front.rest_api.get_api_fn(method, api_object, api_method)()
        result_behind = env.behind.rest_api.get_api_fn(method, api_object, api_method)()
        if result_in_front == result_behind:
            return
        if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
            assert result_in_front == result_behind
        time.sleep(MEDIASERVER_MERGE_TIMEOUT.total_seconds() / 10.0)


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/23920653/Connection+behind+NAT#ConnectionbehindNAT-test_merged_servers_should_return_same_results_to_certain_api_calls
def test_merged_servers_should_return_same_results_to_certain_api_calls(env):
    test_api_calls = [
        ('GET', 'ec2', 'getStorages'),
        ('GET', 'ec2', 'getResourceParams'),
        ('GET', 'ec2', 'getMediaServersEx'),
        ('GET', 'ec2', 'getCamerasEx'),
        ('GET', 'ec2', 'getUsers'),
        ]
    for method, api_object, api_method in test_api_calls:
        wait_for_servers_return_same_results_to_api_call(env, method, api_object, api_method)


def assert_both_servers_are_online(env):
    server_guids = [env.in_front.ecs_guid, env.behind.ecs_guid]
    for server in [env.in_front, env.behind]:
        online_servers = [s for s in server.rest_api.ec2.getMediaServersEx.GET()
                          if s['status'] == 'Online' and s['id'] in server_guids]
        assert len(online_servers) == 2, "'%r' has only %d online servers" % (
            server, len(online_servers))


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/23920653/Connection+behind+NAT#ConnectionbehindNAT-test_proxy_requests
@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('nat_schema', ['nat'])
def test_proxy_requests(env):
    assert_both_servers_are_online(env)
    direct_response_in_front = env.in_front.rest_api.api.moduleInformation.GET()
    direct_response_behind = env.behind.rest_api.api.moduleInformation.GET()
    proxy_response_in_front = env.behind.rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': env.in_front.ecs_guid})
    proxy_response_behind = env.in_front.rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': env.behind.ecs_guid})
    assert direct_response_in_front == proxy_response_in_front
    assert direct_response_behind == proxy_response_behind
    assert direct_response_in_front != direct_response_behind


@pytest.mark.xfail(reason="https://networkoptix.atlassian.net/browse/VMS-7818")
@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('nat_schema', ['nat'])
def test_non_existent_endpoint_via_proxy_with_auth_same_connection(env):
    # Use requests directly, so REST API wrapper doesn't catch errors.
    root_url = env.in_front.rest_api.url.rstrip('/')
    log.debug("Request /api/ping via proxy to show that remote server GUID is correct.")
    requests.get(root_url + '/api/ping', headers={'X-server-guid': env.behind.ecs_guid})
    log.debug("Request non-existent path; both requests are made through same connection.")
    auth = requests.auth.HTTPDigestAuth(env.in_front.rest_api.user, env.in_front.rest_api.password)
    dummy_response = requests.get(root_url + '/dummy', auth=auth, headers={'X-server-guid': env.behind.ecs_guid})
    assert dummy_response.status_code == 404, "Expected 404 but got: %r." % dummy_response


@pytest.mark.xfail(reason="https://networkoptix.atlassian.net/browse/VMS-7819")
@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('nat_schema', ['nat'])
def test_direct_after_proxy_same_connection(env):
    # Use requests directly, so REST API wrapper doesn't catch errors.
    front_url = env.in_front.rest_api.url.rstrip('/') + '/api/ping'
    with requests.Session() as session:
        session.get(front_url, headers={'X-server-guid': env.behind.ecs_guid})
        direct_response = session.get(front_url)
    assert direct_response.status_code == 200, "Expected 200 but got: %r." % direct_response


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
@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('nat_schema', ['nat'])
def test_get_streams(artifact_factory, env, camera, sample_media_file, stream_type):
    env.behind.add_camera(camera)
    start_time_1 = datetime(2017, 1, 27, tzinfo=pytz.utc)
    env.in_front.storage.save_media_sample(camera, start_time_1, sample_media_file)
    env.in_front.rebuild_archive()
    start_time_2 = datetime(2017, 2, 27, tzinfo=pytz.utc)
    env.behind.storage.save_media_sample(camera, start_time_2, sample_media_file)
    env.behind.rebuild_archive()
    assert_server_stream(
        env.behind, camera, sample_media_file, stream_type, artifact_factory, start_time_1)
    assert_server_stream(
        env.in_front, camera, sample_media_file, stream_type, artifact_factory, start_time_1)
    assert_server_stream(
        env.behind, camera, sample_media_file, stream_type, artifact_factory, start_time_2)
    assert_server_stream(
        env.in_front, camera, sample_media_file, stream_type, artifact_factory, start_time_2)
