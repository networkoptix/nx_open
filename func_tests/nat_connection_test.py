import logging
import pytest


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
