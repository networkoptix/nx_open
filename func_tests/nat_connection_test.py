import logging
import pytest
from vagrant_box_config import DEFAULT_HOSTNET


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
    behind_net    = '10.0.7.0'
    behind_net_gw = '10.0.7.200'
    nat = box('nat', ip_address_list=[DEFAULT_HOSTNET, behind_net_gw],
              install_server=False, provision_scripts=['box-provision-nat.sh'])
    behind = box('behind', ip_address_list=[behind_net], provision_scripts=['box-provision-behind-nat.sh'])
    one = server()
    two = server(box=behind)
    return env_builder(http_schema, merge_servers=[one, two], boxes=[nat], one=one, two=two)


def get_rest_api_fn(server, method, api_object, api_method):
    object = getattr(server.rest_api, api_object)  # server.rest_api.ec2
    function = getattr(object, api_method)         # server.rest_api.ec2.getUsers
    return getattr(function, method)               # server.rest_api.ec2.getUsers.get

def test_merged_servers_should_return_same_results_to_certain_api_calls(env):
    test_api_calls = [
        ('get', 'ec2', 'getStorages'),
        ('get', 'ec2', 'getResourceParams'),
        ('get', 'ec2', 'getMediaServersEx'),
        ('get', 'ec2', 'getCamerasEx'),
        ('get', 'ec2', 'getUsers'),
        ]
    for method, api_object, api_method in test_api_calls:
        log.info('TEST for %s %s.%s:', method.upper(), api_object, api_method)
        result_one = get_rest_api_fn(env.one, method, api_object, api_method)()
        result_two = get_rest_api_fn(env.two, method, api_object, api_method)()
        assert result_one == result_two
