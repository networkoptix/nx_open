import logging
from time import sleep

import pytest

from framework.http_api import HttpError
from framework.mediaserver_api import IncompatibleServersMerge
from framework.merging import merge_systems

pytest_plugins = ['fixtures.cloud']

_logger = logging.getLogger(__name__)


def check_user_exists(server, is_cloud):
    users = server.api.generic.get('ec2/getUsers')
    cloud_users = [u for u in users if u['name'] == server.api.generic.http.user]
    assert len(cloud_users) == 1  # One cloud user is expected
    assert cloud_users[0]['isEnabled']
    assert cloud_users[0]['isCloud'] == is_cloud
    if not is_cloud:
        assert len(users) == 1  # No other users are expected for locally setup server


# https://networkoptix.atlassian.net/browse/VMS-3730
# https://networkoptix.atlassian.net/wiki/display/SD/Merge+systems+test#Mergesystemstest-test_with_different_cloud_hosts_must_not_be_able_to_merge
def test_with_different_cloud_hosts_must_not_be_able_to_merge(two_stopped_mediaservers, cloud_account, cloud_host):
    test_cloud_server, wrong_cloud_server = two_stopped_mediaservers

    test_cloud_server.installation.set_cloud_host(cloud_host)
    test_cloud_server.os_access.networking.enable_internet()
    test_cloud_server.start()
    test_cloud_server.api.setup_cloud_system(cloud_account)

    wrong_cloud_server.installation.set_cloud_host('cloud.non.existent')
    wrong_cloud_server.os_access.networking.enable_internet()
    wrong_cloud_server.start()
    wrong_cloud_server.api.setup_local_system()

    check_user_exists(test_cloud_server, is_cloud=True)

    with pytest.raises(IncompatibleServersMerge):
        merge_systems(test_cloud_server, wrong_cloud_server)

    assert not test_cloud_server.installation.list_core_dumps()
    assert not wrong_cloud_server.installation.list_core_dumps()


def test_server_should_be_able_to_merge_local_to_cloud_one(two_stopped_mediaservers, cloud_account, cloud_host):
    cloud_bound_server, local_server = two_stopped_mediaservers

    cloud_bound_server.installation.set_cloud_host(cloud_host)
    cloud_bound_server.os_access.networking.enable_internet()
    cloud_bound_server.start()
    cloud_bound_server.api.setup_cloud_system(cloud_account)
    check_user_exists(cloud_bound_server, is_cloud=True)

    local_server.installation.set_cloud_host(cloud_host)
    local_server.start()
    local_server.api.setup_local_system()
    check_user_exists(local_server, is_cloud=False)

    merge_systems(cloud_bound_server, local_server)
    check_user_exists(local_server, is_cloud=True)

    assert not cloud_bound_server.installation.list_core_dumps()
    assert not local_server.installation.list_core_dumps()


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/85204446/Cloud+test
def test_server_with_hardcoded_cloud_host_should_be_able_to_setup_with_cloud(one_mediaserver, cloud_account):
    one_mediaserver.installation.reset_default_cloud_host()
    one_mediaserver.os_access.networking.enable_internet()
    one_mediaserver.start()
    try:
        one_mediaserver.api.setup_cloud_system(cloud_account)
    except HttpError as x:
        if x.reason == 'Could not connect to cloud: notAuthorized':
            pytest.fail('Mediaserver is incompatible with this cloud host/customization')
        else:
            raise
    check_user_exists(one_mediaserver, is_cloud=True)

    assert not one_mediaserver.installation.list_core_dumps()


def test_setup_cloud_system(one_mediaserver, cloud_account, cloud_host):
    one_mediaserver.installation.set_cloud_host(cloud_host)
    one_mediaserver.os_access.networking.enable_internet()
    one_mediaserver.start()
    one_mediaserver.api.setup_cloud_system(cloud_account)


@pytest.mark.xfail(reason="https://networkoptix.atlassian.net/browse/VMS-9740")
@pytest.mark.parametrize('sleep_sec', [0, 1, 5], ids='sleep_{}s'.format)
def test_setup_cloud_system_enable_internet_after_start(one_mediaserver, cloud_account, sleep_sec, cloud_host):
    one_mediaserver.installation.set_cloud_host(cloud_host)
    one_mediaserver.os_access.networking.disable_internet()
    one_mediaserver.start()
    one_mediaserver.os_access.networking.enable_internet()
    sleep(sleep_sec)
    one_mediaserver.api.setup_cloud_system(cloud_account)
