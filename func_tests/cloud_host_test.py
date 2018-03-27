import logging
from uuid import UUID

import pytest

from test_utils.api_shortcuts import get_cloud_system_id, get_local_system_id
from test_utils.merging import IncompatibleServersMerge, merge_systems, setup_cloud_system, setup_local_system
from test_utils.rest_api import HttpError
from test_utils.server_installation import install_mediaserver

pytest_plugins = ['fixtures.cloud']

log = logging.getLogger(__name__)


def check_user_exists(server, is_cloud):
    users = server.api.ec2.getUsers.GET()
    cloud_users = [u for u in users if u['name'] == server.api.user]
    assert len(cloud_users) == 1  # One cloud user is expected
    assert cloud_users[0]['isEnabled']
    assert cloud_users[0]['isCloud'] == is_cloud
    if not is_cloud:
        assert len(users) == 1  # No other users are expected for locally setup server


# https://networkoptix.atlassian.net/browse/VMS-3730
# https://networkoptix.atlassian.net/wiki/display/SD/Merge+systems+test#Mergesystemstest-test_with_different_cloud_hosts_must_not_be_able_to_merge
def test_with_different_cloud_hosts_must_not_be_able_to_merge(two_linux_servers, cloud_account):
    cloud_host_2 = 'cloud.non.existent'

    one, two = two_linux_servers

    one.start()
    setup_cloud_system(one, cloud_account, {})

    two.installation.patch_binary_set_cloud_host(cloud_host_2)
    two.start()
    setup_local_system(two, {})

    check_user_exists(one, is_cloud=True)

    with pytest.raises(IncompatibleServersMerge):
        merge_systems(one, two)

    # after patching to new cloud host server should reset system and users
    one.stop()
    one.installation.patch_binary_set_cloud_host(cloud_host_2)
    one.start()
    assert get_local_system_id(one.api) == UUID(0), "patch/change cloud host must reset the system"
    setup_local_system(one, {})
    check_user_exists(one, is_cloud=False)  # cloud user must be gone after patch/changed cloud host

    merge_systems(one, two)
    assert not get_cloud_system_id(two.api)
    check_user_exists(two, is_cloud=False)  # cloud user most not get into server two either

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


def test_server_should_be_able_to_merge_local_to_cloud_one(two_linux_servers, cloud_account):
    one, two = two_linux_servers

    one.start()
    setup_cloud_system(one, cloud_account, {})
    check_user_exists(one, is_cloud=True)

    two.start()
    setup_local_system(two, {})
    check_user_exists(two, is_cloud=False)

    merge_systems(one, two)
    check_user_exists(two, is_cloud=True)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/85204446/Cloud+test
def test_server_with_hardcoded_cloud_host_should_be_able_to_setup_with_cloud(
        linux_vm, mediaserver_deb, linux_servers_pool, cloud_account):
    vm = linux_vm.get('original-cloud-host')
    install_mediaserver(vm.os_access, mediaserver_deb, reinstall=True)
    one = linux_servers_pool.get(vm.alias, vm=vm)

    one.start()
    try:
        setup_cloud_system(one, cloud_account, {})
    except HttpError as x:
        if x.reason == 'Could not connect to cloud: notAuthorized':
            pytest.fail('Server is incompatible with this cloud host/customization')
        else:
            raise
    check_user_exists(one, is_cloud=True)

    assert not one.installation.list_core_dumps()
