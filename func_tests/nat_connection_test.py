import logging
import pytest
from datetime import datetime
from test_utils.server import TimePeriod
import pytz


log = logging.getLogger(__name__)


@pytest.fixture(params=['direct', 'nat'])
def nat_schema(request):
    return request.param


@pytest.fixture
def env(env_builder, box, server, http_schema, nat_schema):
    if nat_schema == 'direct':
        env_factory = direct_env
    elif nat_schema == 'nat':
        env_factory = nat_env
    return env_factory(env_builder, box, server, http_schema)


def direct_env(env_builder, box, server, http_schema):
    one = server()
    two = server()
    return env_builder(http_schema, merge_servers=[one, two], one=one, two=two)


def nat_env(env_builder, box, server, http_schema):
    in_front_net    = '10.10.1/24'
    in_front_net_gw = '10.10.1.1/24'
    behind_net      = '10.10.2/24'
    behind_net_gw   = '10.10.2.1/24'
    router = box('router', ip_address_list=[in_front_net_gw, behind_net_gw],
                 install_server=False, provision_scripts=['box-provision-nat-router.sh'])
    behind = box('behind', ip_address_list=[behind_net], provision_scripts=['box-provision-nat-behind.sh'])
    in_front = box('in-front', ip_address_list=[in_front_net])
    one = server(box=in_front)
    two = server(box=behind)
    # two must go first in merge_servers, it can reach sever one, but not vise-versa
    return env_builder(http_schema, merge_servers=[two, one], boxes=[router], one=one, two=two)


def test_merged_servers_should_return_same_results_to_certain_api_calls(env):
    test_api_calls = [
        ('GET', 'ec2', 'getStorages'),
        ('GET', 'ec2', 'getResourceParams'),
        ('GET', 'ec2', 'getMediaServersEx'),
        ('GET', 'ec2', 'getCamerasEx'),
        ('GET', 'ec2', 'getUsers'),
        ]
    for method, api_object, api_method in test_api_calls:
        log.info('TEST for %s %s.%s:', method, api_object, api_method)
        result_one = env.one.rest_api.get_api_fn(method, api_object, api_method)()
        result_two = env.two.rest_api.get_api_fn(method, api_object, api_method)()
        assert result_one == result_two


def assert_both_servers_are_online(env):
    server_guids = [env.one.ecs_guid, env.two.ecs_guid]
    for server in [env.one, env.two]:
        online_servers = [s for s in server.rest_api.ec2.getMediaServersEx.GET()
                          if s['status'] == 'Online' and s['id'] in server_guids]
        assert len(online_servers) == 2, "'%r' has only %d online servers" % (
            server.box, len(online_servers))


@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('nat_schema', ['nat'])
def test_proxy_requests(env):
    assert_both_servers_are_online(env)
    direct_response_one = env.one.rest_api.api.moduleInformation.GET()
    direct_response_two = env.two.rest_api.api.moduleInformation.GET()
    proxy_response_one = env.two.rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': env.one.ecs_guid})
    proxy_response_two = env.one.rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': env.two.ecs_guid})
    assert direct_response_one == proxy_response_one
    assert direct_response_two == proxy_response_two
    assert direct_response_one != direct_response_two


def assert_server_stream(server, camera, sample_media_file, stream_type, artifact_path_prefix, start_time):
    assert TimePeriod(start_time, sample_media_file.duration) in server.get_recorded_time_periods(camera)
    stream = server.get_media_stream(stream_type, camera)
    metadata_list = stream.load_archive_stream_metadata(
        '%s-stream-media-%s' % (artifact_path_prefix, stream_type),
        pos=start_time, duration=sample_media_file.duration)
    for metadata in metadata_list:
        assert metadata.width == sample_media_file.width
        assert metadata.height == sample_media_file.height


@pytest.mark.parametrize('http_schema', ['http'])
@pytest.mark.parametrize('nat_schema', ['nat'])
def test_get_streams(env, camera, sample_media_file, stream_type):
    env.two.add_camera(camera)
    start_time_1 = datetime(2017, 1, 27, tzinfo=pytz.utc)
    env.one.storage.save_media_sample(camera, start_time_1, sample_media_file)
    env.one.rebuild_archive()
    start_time_2 = datetime(2017, 2, 27, tzinfo=pytz.utc)
    env.two.storage.save_media_sample(camera, start_time_2, sample_media_file)
    env.two.rebuild_archive()
    assert_server_stream(
        env.two, camera, sample_media_file, stream_type, env.artifact_path_prefix, start_time_1)
    assert_server_stream(
        env.one, camera, sample_media_file, stream_type, env.artifact_path_prefix, start_time_1)
    assert_server_stream(
        env.two, camera, sample_media_file, stream_type, env.artifact_path_prefix, start_time_2)
    assert_server_stream(
        env.one, camera, sample_media_file, stream_type, env.artifact_path_prefix, start_time_2)
