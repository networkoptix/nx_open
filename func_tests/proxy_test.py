"""Test proxy request feature (request with X-server-guid HTTP header)."""


def test_proxy_light_request(server_factory, http_schema):
    one = server_factory('one', http_schema=http_schema)
    two = server_factory('two', http_schema=http_schema)
    one.merge([two])

    response1 = one.rest_api.api.moduleInformation.GET()
    response2_proxy = two.rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': one.ecs_guid})
    assert response1 == response2_proxy
    response2 = two.rest_api.api.moduleInformation.GET()
    response1_proxy = one.rest_api.api.moduleInformation.GET(
        headers={'X-server-guid': two.ecs_guid})
    assert response1_proxy == response2


def test_proxy_hard_request(server_factory, http_schema):
    one = server_factory('one', http_schema=http_schema)
    two = server_factory('two', http_schema=http_schema)
    one.merge([two])

    response1 = one.rest_api.ec2.getFullInfo.GET()
    response2 = two.rest_api.ec2.getFullInfo.GET()
    assert response1 == response2
    response1_proxy = one.rest_api.ec2.getFullInfo.GET(
        headers={'X-server-guid': two.ecs_guid})
    assert response2 == response1_proxy


def test_proxy_request_to_another_network(box, server_factory, http_schema):
    net_1 = '10.10.1/24'
    net_2 = '10.10.2/24'
    box_1 = box('side', ip_address_list=[net_1])
    box_2 = box('side', ip_address_list=[net_2])
    middle_box = box('middle', ip_address_list=[net_1, net_2])
    one = server_factory('one', box=box_1, http_schema=http_schema)
    two = server_factory('two', box=box_2, http_schema=http_schema)
    middle = server_factory('middle', box=middle_box, http_schema=http_schema)
    middle.merge([one, two])

    test_response = one.rest_api.ec2.testConnection.GET(
        headers={'X-server-guid': two.ecs_guid})
    assert test_response['ecsGuid'] == two.ecs_guid
    servers = one.rest_api.ec2.getMediaServersEx.GET(
        id=two.ecs_guid,
        headers={'X-server-guid': two.ecs_guid})
    assert len(servers) == 1
    assert servers[0]['status'] == 'Online'
