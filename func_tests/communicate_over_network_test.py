import logging
import time
from datetime import datetime

import pytest
import pytz
import requests
import requests.auth

from test_utils.networking import setup_networks
from test_utils.networking.linux import LinuxNodeNetworking
from test_utils.server import MEDIASERVER_MERGE_TIMEOUT, TimePeriod
from test_utils.utils import datetime_utc_now

log = logging.getLogger(__name__)

LAYOUTS = {
    'not routed': {
        'mediaservers': ['middle', 'first', 'second'],  # Order matters!
        'default_gateways': {'first': 'internet', 'middle': 'internet', 'second': 'internet'},
        'networks': {
            '10.254.254.0/28': {
                'first': None,
                'middle': None},
            '10.254.254.16/28': {
                'middle': None,
                'second': None}}},
    'nat': {
        'mediaservers': ['first', 'second'],
        'default_gateways': {'first': 'internet', 'second': 'internet'},
        'networks': {
            '10.254.254.0/28': {
                'first': None,
                'router': {
                    '10.254.254.16/28': {
                        'second': None}}}}},
    'one network': {
        'mediaservers': ['first', 'second'],
        'default_gateways': {'first': 'internet', 'second': 'internet'},
        'networks': {
            '10.254.254.0/28': {
                'first': None,
                'second': None}}}}


@pytest.fixture()
def env(vm_factory, server_factory, http_schema, layout_name):
    layout = LAYOUTS[layout_name]
    vms, _ = setup_networks([vm_factory() for _ in range(3)], layout['networks'], layout['default_gateways'])
    servers = {}
    for alias in layout['mediaservers']:
        servers[alias] = server_factory('server', vm=vms[alias], http_schema=http_schema)
    first_server = servers[layout['mediaservers'][0]]
    other_servers = [servers[alias] for alias in layout['mediaservers'][1:]]
    first_server.merge(other_servers)
    return servers


def wait_for_servers_return_same_results_to_api_call(env, method, api_object, api_method):
    log.info('TEST for %s %s.%s:', method, api_object, api_method)
    start_time = datetime_utc_now()
    while True:
        result_front = env['first'].rest_api.get_api_fn(method, api_object, api_method)()
        result_behind = env['second'].rest_api.get_api_fn(method, api_object, api_method)()
        if result_front == result_behind:
            return
        if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
            assert result_front == result_behind
        time.sleep(MEDIASERVER_MERGE_TIMEOUT.total_seconds() / 10.0)


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/23920653/Connection+behind+NAT#ConnectionbehindNAT-test_merged_servers_should_return_same_results_to_certain_api_calls
@pytest.mark.parametrize('http_schema', ['http', 'https'])
@pytest.mark.parametrize('layout_name', ['one network', 'nat'])
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
    server_guids = [env['first'].ecs_guid, env['second'].ecs_guid]
    for server in [env['first'], env['second']]:
        online_servers = [s for s in server.rest_api.ec2.getMediaServersEx.GET()
                          if s['status'] == 'Online' and s['id'] in server_guids]
        assert len(online_servers) == 2, "'%r' has only %d online servers" % (
            server, len(online_servers))


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/23920653/Connection+behind+NAT#ConnectionbehindNAT-test_proxy_requests
@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('layout_name', ['nat'])
def test_proxy_requests(env):
    assert_both_servers_are_online(env)
    direct_response_front = env['first'].rest_api.api.moduleInformation.GET()
    direct_response_behind = env['second'].rest_api.api.moduleInformation.GET()
    proxy_response_front = env['second'].rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': env['first'].ecs_guid})
    proxy_response_behind = env['first'].rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': env['second'].ecs_guid})
    assert direct_response_front == proxy_response_front
    assert direct_response_behind == proxy_response_behind
    assert direct_response_front != direct_response_behind


@pytest.mark.xfail(reason="https://networkoptix.atlassian.net/browse/VMS-7818")
@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('layout_name', ['nat'])
def test_non_existent_endpoint_via_proxy_with_auth_same_connection(env):
    # Use requests directly, so REST API wrapper doesn't catch errors.
    root_url = env['first'].rest_api.url.rstrip('/')
    log.debug("Request /api/ping via proxy to show that remote server GUID is correct.")
    requests.get(root_url + '/api/ping', headers={'X-server-guid': env['second'].ecs_guid})
    log.debug("Request non-existent path; both requests are made through same connection.")
    auth = requests.auth.HTTPDigestAuth(env['first'].rest_api.user, env['first'].rest_api.password)
    dummy_response = requests.get(root_url + '/dummy', auth=auth, headers={'X-server-guid': env['second'].ecs_guid})
    assert dummy_response.status_code == 404, "Expected 404 but got: %r." % dummy_response


@pytest.mark.xfail(reason="https://networkoptix.atlassian.net/browse/VMS-7819")
@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('layout_name', ['nat'])
def test_direct_after_proxy_same_connection(env):
    # Use requests directly, so REST API wrapper doesn't catch errors.
    front_url = env['first'].rest_api.url.rstrip('/') + '/api/ping'
    with requests.Session() as session:
        session.get(front_url, headers={'X-server-guid': env['second'].ecs_guid})
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


@pytest.mark.quick
@pytest.mark.xfail(reason="https://networkoptix.atlassian.net/browse/VMS-8259")
@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('layout_name', ['nat'])
def test_proxy_light_request(env):
    response1 = env['first'].rest_api.api.moduleInformation.GET()
    response2_proxy = env['second'].rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': env['first'].ecs_guid})
    assert response1 == response2_proxy
    response2 = env['second'].rest_api.api.moduleInformation.GET()
    response1_proxy = env['first'].rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': env['second'].ecs_guid})
    assert response1_proxy == response2


@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('layout_name', ['one network'])
def test_proxy_hard_request(env):
    response1 = env['first'].rest_api.ec2.getFullInfo.GET()
    response2 = env['second'].rest_api.ec2.getFullInfo.GET()
    assert response1 == response2
    response1_proxy = env['first'].rest_api.ec2.getFullInfo.GET(
        headers={'X-server-guid': env['second'].ecs_guid})
    assert response2 == response1_proxy


@pytest.mark.quick
@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('layout_name', ['not routed'])
def test_proxy_request_to_another_network(env):
    test_response = env['first'].rest_api.ec2.testConnection.GET(
        headers={'X-server-guid': env['second'].ecs_guid})
    assert test_response['ecsGuid'] == env['second'].ecs_guid
    servers = env['first'].rest_api.ec2.getMediaServersEx.GET(
        id=env['second'].ecs_guid,
        headers={'X-server-guid': env['second'].ecs_guid})
    assert len(servers) == 1
    assert servers[0]['status'] == 'Online'


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/23920653/Connection+behind+NAT#ConnectionbehindNAT-test_get_streams
@pytest.mark.slow
@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('layout_name', ['nat'])
def test_get_streams(artifact_factory, env, camera, sample_media_file, stream_type):
    env['second'].add_camera(camera)
    start_time_1 = datetime(2017, 1, 27, tzinfo=pytz.utc)
    env['first'].storage.save_media_sample(camera, start_time_1, sample_media_file)
    env['first'].rebuild_archive()
    start_time_2 = datetime(2017, 2, 27, tzinfo=pytz.utc)
    env['second'].storage.save_media_sample(camera, start_time_2, sample_media_file)
    env['second'].rebuild_archive()
    assert_server_stream(
        env['second'], camera, sample_media_file, stream_type, artifact_factory, start_time_1)
    assert_server_stream(
        env['first'], camera, sample_media_file, stream_type, artifact_factory, start_time_1)
    assert_server_stream(
        env['second'], camera, sample_media_file, stream_type, artifact_factory, start_time_2)
    assert_server_stream(
        env['first'], camera, sample_media_file, stream_type, artifact_factory, start_time_2)
