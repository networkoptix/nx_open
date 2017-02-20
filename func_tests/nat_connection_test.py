import logging
import pytest


log = logging.getLogger(__name__)


@pytest.fixture
def env(env_builder, server, http_schema):
    one = server()
    two = server()
    return env_builder(http_schema, merge_servers=[one, two], one=one, two=two)


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
