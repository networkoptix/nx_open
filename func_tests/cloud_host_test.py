import logging

import pytest

from test_utils.rest_api import HttpError, ServerRestApiError

log = logging.getLogger(__name__)


def check_user_exists(server, is_cloud):
    users = server.rest_api.ec2.getUsers.GET()
    cloud_users = [u for u in users if u['name'] == server.user]
    assert len(cloud_users) == 1  # One cloud user is expected
    assert cloud_users[0]['isEnabled']
    assert cloud_users[0]['isCloud'] == is_cloud
    if not is_cloud:
        assert len(users) == 1  # No other users are expected for locally setup server

# https://networkoptix.atlassian.net/browse/VMS-3730
# https://networkoptix.atlassian.net/wiki/display/SD/Merge+systems+test#Mergesystemstest-test_with_different_cloud_hosts_must_not_be_able_to_merge
def test_with_different_cloud_hosts_must_not_be_able_to_merge(server_factory, cloud_account, http_schema):
    cloud_host_2 = 'cloud.non.existent'

    one = server_factory('one', setup=False, http_schema=http_schema)
    two = server_factory('two', start=False, http_schema=http_schema)

    two.patch_binary_set_cloud_host(cloud_host_2)
    two.start_service()
    two.setup_local_system()

    one.setup_cloud_system(cloud_account)
    check_user_exists(one, is_cloud=True)

    with pytest.raises(ServerRestApiError) as x_info:
        one.merge_systems(two)
    assert x_info.value.error_string == 'INCOMPATIBLE'

    # after patching to new cloud host server should reset system and users
    one.stop_service()
    one.patch_binary_set_cloud_host(cloud_host_2)
    one.start_service()
    assert one.get_setup_type() == None  # patch/change cloud host must reset the system
    one.setup_local_system()
    check_user_exists(one, is_cloud=False)  # cloud user must be gone after patch/changed cloud host

    one.merge_systems(two)
    assert two.get_setup_type() == 'local'
    check_user_exists(two, is_cloud=False)  # cloud user most not get into server two either
    

def test_server_should_be_able_to_merge_local_to_cloud_one(server_factory, cloud_account, http_schema):
    one = server_factory('one', setup=False, http_schema=http_schema)
    two = server_factory('two', http_schema=http_schema)

    one.setup_cloud_system(cloud_account)
    check_user_exists(one, is_cloud=True)

    check_user_exists(two, is_cloud=False)
    one.merge_systems(two)
    check_user_exists(two, is_cloud=True)


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/85204446/Cloud+test
def test_server_with_hardcoded_cloud_host_should_be_able_to_setup_with_cloud(server_factory, cloud_account, http_schema):
    one = server_factory('one', setup=False, leave_initial_cloud_host=True, http_schema=http_schema)
    try:
        one.setup_cloud_system(cloud_account)
    except HttpError as x:
        if x.reason == 'Could not connect to cloud: notAuthorized':
            pytest.fail('Server is incompatible with this cloud host/customization')
        else:
            raise
    check_user_exists(one, is_cloud=True)
