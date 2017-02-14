# $Id$
# Artem V. Nikitin
# Test inter-server redirects

import pytest
from test_utils import print_list

@pytest.fixture
def env(env_builder, server, http_schema):
    one = server()
    two = server()
    return env_builder(http_schema, merge_servers=[one, two], one=one, two=two)

# Use light 'api/moduleInformation' request for testing
def test_proxy_light_request(env):
    response1 = env.one.rest_api.api.moduleInformation.get()
    response2_proxy = env.two.rest_api.api.moduleInformation.get(
        headers = {'X-server-guid': env.one.ecs_guid })
    assert response1 == response2_proxy
    response2 = env.two.rest_api.api.moduleInformation.get()
    response1_proxy = env.one.rest_api.api.moduleInformation.get(
        headers = {'X-server-guid': env.two.ecs_guid })
    assert response1_proxy == response2

# Use hard 'ec2/getFullInfo' request for testing
def test_proxy_hard_request(env):
    response1 = env.one.rest_api.ec2.getFullInfo.get()
    response2 = env.two.rest_api.ec2.getFullInfo.get()
    assert response1 == response2
    response1_proxy = env.one.rest_api.ec2.getFullInfo.get(
        headers = {'X-server-guid': env.two.ecs_guid })
    assert response2 == response1_proxy
