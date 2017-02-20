import logging
import time
import pprint
import pytest
import pytz
from server_rest_api import ServerRestApiError
from test_utils import print_list


log = logging.getLogger(__name__)



def check_user_exists(server, is_cloud):
    users = server.rest_api.ec2.getUsers.get()
    cloud_users = [u for u in users if u['name'] == server.user]
    assert len(cloud_users) == 1  # One cloud user is expected
    assert cloud_users[0]['isEnabled']
    assert cloud_users[0]['isCloud'] == is_cloud
    if not is_cloud:
        assert len(users) == 1  # No other users are expected for locally setup server


@pytest.fixture
def env(env_builder, server, http_schema):
    one = server(setup=False)
    two = server(start=False)
    return env_builder(http_schema, one=one, two=two)


# https://networkoptix.atlassian.net/browse/VMS-3730
# https://networkoptix.atlassian.net/wiki/display/SD/Merge+systems+test#Mergesystemstest-test_with_different_cloud_hosts_must_not_be_able_to_merge
def test_with_different_cloud_hosts_must_not_be_able_to_merge(env):
    cloud_host = 'cloud.non.existent'

    env.two.patch_binary_set_cloud_host(cloud_host)
    env.two.start_service()
    env.two.setup_local_system()

    env.one.setup_cloud_system()
    check_user_exists(env.one, is_cloud=True)

    with pytest.raises(ServerRestApiError) as x_info:
        env.one.merge_systems(env.two)
    assert x_info.value.error_string == 'INCOMPATIBLE'

    env.one.stop_service()
    env.one.patch_binary_set_cloud_host(cloud_host)
    env.one.start_service()
    assert env.one.get_setup_type() == None  # patch/change cloud host must reset the system
    env.one.setup_local_system()
    check_user_exists(env.one, is_cloud=False)  # cloud user must be gone after patch/changed cloud host

    env.one.merge_systems(env.two)
    assert env.two.get_setup_type() == 'local'
    check_user_exists(env.two, is_cloud=False)  # cloud user most not get into server two either
    

@pytest.fixture
def env1(env_builder, server):
    one = server(setup=False)
    two = server()
    return env_builder(one=one, two=two)

def test_server_should_be_able_to_merge_local_to_cloud_one(env1):
    env1.one.setup_cloud_system()
    check_user_exists(env1.one, is_cloud=True)

    check_user_exists(env1.two, is_cloud=False)
    env1.one.merge_systems(env1.two)
    check_user_exists(env1.two, is_cloud=True)
