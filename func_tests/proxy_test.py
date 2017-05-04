'''
Proxy test. It tests proxy request feature (request with X-server-guid HTTP header)
'''

import pytest


@pytest.fixture
def env(env_builder, server, http_schema):
    one = server()
    two = server()
    return env_builder(http_schema, merge_servers=[one, two], one=one, two=two)


def test_proxy_light_request(env):
    response1 = env.one.rest_api.api.moduleInformation.GET()
    response2_proxy = env.two.rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': env.one.ecs_guid})
    assert response1 == response2_proxy
    response2 = env.two.rest_api.api.moduleInformation.GET()
    response1_proxy = env.one.rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': env.two.ecs_guid})
    assert response1_proxy == response2


def test_proxy_hard_request(env):
    response1 = env.one.rest_api.ec2.getFullInfo.GET()
    response2 = env.two.rest_api.ec2.getFullInfo.GET()
    assert response1 == response2
    response1_proxy = env.one.rest_api.ec2.getFullInfo.GET(
        headers={'X-server-guid': env.two.ecs_guid})
    assert response2 == response1_proxy


@pytest.fixture
def env_networks(env_builder, box, server):
    net_1 = '10.10.1/24'
    net_2 = '10.10.2/24'
    box_1 = box('side', ip_address_list=[net_1])
    box_2 = box('side', ip_address_list=[net_2])
    middle_box = box('middle', ip_address_list=[net_1, net_2])
    one = server(box=box_1)
    two = server(box=box_2)
    middle = server(box=middle_box)
    return env_builder(one=one, two=two, middle=middle)


def test_proxy_request_to_another_network(env_networks):
    env = env_networks
    env.middle.merge_systems(env.one)
    env.middle.merge_systems(env.two)
    test_response = env.one.rest_api.ec2.testConnection.GET(
        headers={'X-server-guid': env.two.ecs_guid})
    assert test_response['ecsGuid'] == env.two.ecs_guid
    servers = env.one.rest_api.ec2.getMediaServersEx.GET(
        id=env.two.ecs_guid,
        headers={'X-server-guid': env.two.ecs_guid})
    assert len(servers) == 1
    assert servers[0]['status'] == 'Online'
