import logging

import pytest

from framework.dpkg_installation import install_mediaserver
from framework.mediaserver import Mediaserver
from framework.mediaserver_factory import cleanup_mediaserver
from framework.merging import IncompatibleServersMerge, merge_systems, setup_cloud_system, setup_local_system
from framework.rest_api import HttpError, RestApi
from framework.service import UpstartService

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
def test_with_different_cloud_hosts_must_not_be_able_to_merge(two_linux_mediaservers, cloud_account):
    test_cloud_server, wrong_cloud_server = two_linux_mediaservers

    test_cloud_server.start()
    test_cloud_server.machine.networking.enable_internet()
    setup_cloud_system(test_cloud_server, cloud_account, {})

    wrong_cloud_server.installation.patch_binary_set_cloud_host('cloud.non.existent')
    wrong_cloud_server.start()
    setup_local_system(wrong_cloud_server, {})

    check_user_exists(test_cloud_server, is_cloud=True)

    with pytest.raises(IncompatibleServersMerge):
        merge_systems(test_cloud_server, wrong_cloud_server)

    assert not test_cloud_server.installation.list_core_dumps()
    assert not wrong_cloud_server.installation.list_core_dumps()


def test_server_should_be_able_to_merge_local_to_cloud_one(two_linux_mediaservers, cloud_account):
    cloud_bound_server, local_server = two_linux_mediaservers

    cloud_bound_server.start()
    cloud_bound_server.machine.networking.enable_internet()
    setup_cloud_system(cloud_bound_server, cloud_account, {})
    check_user_exists(cloud_bound_server, is_cloud=True)

    local_server.start()
    setup_local_system(local_server, {})
    check_user_exists(local_server, is_cloud=False)

    merge_systems(cloud_bound_server, local_server)
    check_user_exists(local_server, is_cloud=True)

    assert not cloud_bound_server.installation.list_core_dumps()
    assert not local_server.installation.list_core_dumps()


@pytest.fixture()
def original_cloud_host_server(linux_vm, mediaserver_deb, ca):
    installation = install_mediaserver(linux_vm.os_access, mediaserver_deb, reinstall=True)
    service = UpstartService(linux_vm.os_access, mediaserver_deb.customization.service)
    name = 'original-cloud-host'
    api = RestApi(name, *linux_vm.ports['tcp', 7001])
    mediaserver = Mediaserver(name, service, installation, api, linux_vm, 7001)
    cleanup_mediaserver(mediaserver, ca)
    mediaserver.start()
    return mediaserver


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/85204446/Cloud+test
def test_server_with_hardcoded_cloud_host_should_be_able_to_setup_with_cloud(original_cloud_host_server, cloud_account):
    try:
        original_cloud_host_server.machine.networking.enable_internet()
        setup_cloud_system(original_cloud_host_server, cloud_account, {})
    except HttpError as x:
        if x.reason == 'Could not connect to cloud: notAuthorized':
            pytest.fail('Mediaserver is incompatible with this cloud host/customization')
        else:
            raise
    check_user_exists(original_cloud_host_server, is_cloud=True)

    assert not original_cloud_host_server.installation.list_core_dumps()
