"""Merge test

Initial task https://networkoptix.atlassian.net/browse/TEST-177
"""

import logging
import time

import pytest

import server_api_data_generators as generator
from framework.http_api import HttpError
from framework.installation.cloud_host_patching import set_cloud_host
from framework.installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from framework.mediaserver_api import INITIAL_API_PASSWORD
from framework.merging import (
    ExplicitMergeError,
    merge_systems,
    )
from framework.utils import bool_to_str, datetime_utc_now
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)


@pytest.fixture()
def test_system_settings():
    return {'cameraSettingsOptimization': 'false', 'autoDiscoveryEnabled': 'false', 'statisticsAllowed': 'false'}


def check_system_settings(server, **kw):
    settings_to_check = {k: v for k, v in server.api.get_system_settings().items() if k in kw.keys()}
    assert settings_to_check == kw


def wait_for_settings_merge(one, two):
    wait_for_true(
        lambda: one.api.get_system_settings() == two.api.get_system_settings(),
        '{} and {} response identically to /api/systemSettings'.format(one, two))


def check_admin_disabled(server):
    users = server.api.generic.get('ec2/getUsers')
    admin_users = [u for u in users if u['name'] == 'admin']
    assert len(admin_users) == 1  # One cloud user is expected
    assert not admin_users[0]['isEnabled']
    with pytest.raises(HttpError) as x_info:
        server.api.generic.post('ec2/saveUser', dict(
            id=admin_users[0]['id'],
            isEnabled=True))
    assert x_info.value.status_code == 403


@pytest.fixture()
def one(two_stopped_mediaservers, test_system_settings, cloud_host):
    one, _ = two_stopped_mediaservers
    set_cloud_host(one.installation, cloud_host)
    one.os_access.networking.enable_internet()
    one.start()
    one.api.setup_local_system(test_system_settings)
    return one


@pytest.fixture()
def two(two_stopped_mediaservers, cloud_host):
    _, two = two_stopped_mediaservers
    set_cloud_host(two.installation, cloud_host)
    two.os_access.networking.enable_internet()
    two.start()
    two.api.setup_local_system()
    return two


@pytest.mark.local
def test_simplest_merge(two_separate_mediaservers):
    one, two = two_separate_mediaservers
    merge_systems(one, two)
    assert one.api.get_local_system_id() == two.api.get_local_system_id()
    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


@pytest.mark.local
def test_merge_take_local_settings(one, two, test_system_settings):
    # Start two servers without predefined systemName
    # and move them to working state
    check_system_settings(one, **test_system_settings)

    # On each server update some globalSettings to different values
    expected_arecont_rtsp_enabled = one.api.toggle_system_setting('arecontRtspEnabled')
    expected_audit_trail_enabled = not two.api.toggle_system_setting('auditTrailEnabled')

    # Merge systems (takeRemoteSettings = false)
    merge_systems(two, one)
    wait_for_settings_merge(one, two)
    check_system_settings(
        one,
        arecontRtspEnabled=bool_to_str(expected_arecont_rtsp_enabled),
        auditTrailEnabled=bool_to_str(expected_audit_trail_enabled))

    # Ensure both servers are merged and sync
    expected_arecont_rtsp_enabled = not one.api.toggle_system_setting('arecontRtspEnabled')
    wait_for_settings_merge(one, two)
    check_system_settings(
        two,
        arecontRtspEnabled=bool_to_str(expected_arecont_rtsp_enabled))

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


# Start two servers without predefined systemName
# and move them to working state
@pytest.mark.local
def test_merge_take_remote_settings(one, two):
    # On each server update some globalSettings to different values
    expected_arecont_rtsp_enabled = not one.api.toggle_system_setting('arecontRtspEnabled')
    expected_audit_trail_enabled = not two.api.toggle_system_setting('auditTrailEnabled')

    # Merge systems (takeRemoteSettings = true)
    merge_systems(two, one, take_remote_settings=True)
    wait_for_settings_merge(one, two)
    check_system_settings(
        one,
        arecontRtspEnabled=bool_to_str(expected_arecont_rtsp_enabled),
        auditTrailEnabled=bool_to_str(expected_audit_trail_enabled))

    # Ensure both servers are merged and sync
    expected_audit_trail_enabled = not one.api.toggle_system_setting('auditTrailEnabled')
    wait_for_settings_merge(one, two)
    check_system_settings(
        two,
        auditTrailEnabled=bool_to_str(expected_audit_trail_enabled))

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


def test_merge_cloud_with_local(two_stopped_mediaservers, cloud_account, test_system_settings, cloud_host):
    one, two = two_stopped_mediaservers

    set_cloud_host(one.installation, cloud_host)
    one.os_access.networking.enable_internet()
    one.start()
    one.api.setup_cloud_system(cloud_account, test_system_settings)

    set_cloud_host(two.installation, cloud_host)
    two.start()
    two.api.setup_local_system()

    # Merge systems (takeRemoteSettings = False) -> Error
    two.os_access.networking.enable_internet()
    try:
        merge_systems(two, one)
    except ExplicitMergeError as e:
        assert e.error_string == 'DEPENDENT_SYSTEM_BOUND_TO_CLOUD'
    else:
        assert False, 'Expected: DEPENDENT_SYSTEM_BOUND_TO_CLOUD'

    # Merge systems (takeRemoteSettings = true)
    merge_systems(two, one, take_remote_settings=True)
    wait_for_settings_merge(one, two)
    check_system_settings(two, **test_system_settings)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/71467018/Merge+systems+test#Mergesystemstest-test_merge_cloud_systems
@pytest.mark.parametrize('take_remote_settings', [True, False], ids=['settings_from_remote', 'settings_from_local'])
def test_merge_cloud_systems(two_stopped_mediaservers, cloud_account_factory, take_remote_settings, cloud_host):
    cloud_account_1 = cloud_account_factory()
    cloud_account_2 = cloud_account_factory()

    one, two = two_stopped_mediaservers

    set_cloud_host(one.installation, cloud_host)
    one.os_access.networking.enable_internet()
    one.start()
    one.api.setup_cloud_system(cloud_account_1)

    set_cloud_host(two.installation, cloud_host)
    two.os_access.networking.enable_internet()
    two.start()
    two.api.setup_cloud_system(cloud_account_2)

    # Merge 2 cloud systems one way
    try:
        merge_systems(one, two, take_remote_settings=take_remote_settings)
    except ExplicitMergeError as e:
        assert e.error_string == 'BOTH_SYSTEM_BOUND_TO_CLOUD'
    else:
        assert False, 'Expected: BOTH_SYSTEM_BOUND_TO_CLOUD'

    # Test default (admin) user disabled
    check_admin_disabled(one)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


def test_cloud_merge_after_disconnect(two_stopped_mediaservers, cloud_account, test_system_settings, cloud_host):
    one, two = two_stopped_mediaservers

    set_cloud_host(one.installation, cloud_host)
    one.os_access.networking.enable_internet()
    one.start()
    one.api.setup_cloud_system(cloud_account, test_system_settings)

    set_cloud_host(two.installation, cloud_host)
    two.os_access.networking.enable_internet()
    two.start()
    two.api.setup_cloud_system(cloud_account)

    # Check setupCloud's settings on Server1
    check_system_settings(one, **test_system_settings)

    # Disconnect Server2 from cloud
    new_password = 'new_password'
    two.api.detach_from_cloud(new_password)

    # Merge systems (takeRemoteSettings = true)
    merge_systems(two, one, take_remote_settings=True)
    wait_for_settings_merge(one, two)

    # Ensure both servers are merged and sync
    expected_audit_trail_enabled = not one.api.toggle_system_setting('auditTrailEnabled')
    wait_for_settings_merge(one, two)
    check_system_settings(
        two,
        auditTrailEnabled=bool_to_str(expected_audit_trail_enabled))

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


def wait_entity_merge_done(one, two, endpoint, expected_resources):
    start_time = datetime_utc_now()
    while True:
        result_1 = one.api.generic.get(endpoint)
        result_2 = two.api.generic.get(endpoint)
        if result_1 == result_2:
            got_resources = [v['id'] for v in result_1 if v['id'] in expected_resources]
            assert got_resources == expected_resources
            return
        if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
            assert result_1 == result_2
        time.sleep(MEDIASERVER_MERGE_TIMEOUT.total_seconds() / 10.0)


@pytest.mark.local
def test_merge_resources(two_separate_mediaservers):
    one, two = two_separate_mediaservers
    user_data = generator.generate_user_data(1)
    camera_data = generator.generate_camera_data(1)
    one.api.generic.post('ec2/saveUser', dict(**user_data))
    two.api.generic.post('ec2/saveCamera', dict(**camera_data))
    merge_systems(two, one)
    wait_entity_merge_done(one, two, 'ec2/getUsers', [user_data['id']])
    wait_entity_merge_done(one, two, 'ec2/getCamerasEx', [camera_data['id']])

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


@pytest.mark.cloud
def test_restart_one_server(one, two, cloud_account, ca):
    merge_systems(one, two)

    # Stop Server2 and clear its database
    guid2 = two.api.get_server_id()
    two.stop()
    two.installation.cleanup(ca.generate_key_and_cert())
    two.start()

    # Remove Server2 from database on Server1
    one.api.generic.post('ec2/removeResource', dict(id=guid2))
    # Restore initial REST API
    two.api.generic.http.set_credentials('admin', INITIAL_API_PASSWORD)

    # Start server 2 again and move it from initial to working state
    two.api.setup_cloud_system(cloud_account)
    two.api.generic.get('ec2/getUsers')

    # Merge systems (takeRemoteSettings = false)
    merge_systems(two, one)
    two.api.generic.get('ec2/getUsers')

    # Ensure both servers are merged and sync
    expected_arecont_rtsp_enabled = not one.api.toggle_system_setting('arecontRtspEnabled')
    wait_for_settings_merge(one, two)
    check_system_settings(
        two,
        arecontRtspEnabled=bool_to_str(expected_arecont_rtsp_enabled))

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()
