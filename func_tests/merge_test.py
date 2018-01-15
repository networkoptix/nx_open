"""Merge test

Initial task https://networkoptix.atlassian.net/browse/TEST-177
"""

import logging
import time

import pytest

import server_api_data_generators as generator
from test_utils.rest_api import RestApiError, HttpError
from test_utils.server import MEDIASERVER_MERGE_TIMEOUT
from test_utils.utils import bool_to_str, str_to_bool, datetime_utc_now

log = logging.getLogger(__name__)


@pytest.fixture
def test_system_settings():
    return dict(systemSettings=dict(
        cameraSettingsOptimization=bool_to_str(False),
        autoDiscoveryEnabled=bool_to_str(False),
        statisticsAllowed=bool_to_str(False)))


def check_system_settings(server, **kw):
    settings_to_check = {k: v for k, v in server.settings.iteritems() if k in kw.keys()}
    assert settings_to_check == kw


def change_bool_setting(server, setting):
    val = str_to_bool(server.settings[setting])
    settings = {setting:  bool_to_str(not val)}
    server.set_system_settings(**settings)
    check_system_settings(server, **settings)
    return val


def wait_for_settings_merge(one, two):
    start_time = datetime_utc_now()
    while datetime_utc_now() - start_time < MEDIASERVER_MERGE_TIMEOUT:
        one.load_system_settings()
        two.load_system_settings()
        if one.settings == two.settings:
            return
        time.sleep(0.5)
    assert one.settings == two.settings


def check_admin_disabled(server):
    users = server.rest_api.ec2.getUsers.GET()
    admin_users = [u for u in users if u['name'] == 'admin']
    assert len(admin_users) == 1  # One cloud user is expected
    assert not admin_users[0]['isEnabled']
    with pytest.raises(HttpError) as x_info:
        server.rest_api.ec2.saveUser.POST(
            id=admin_users[0]['id'],
            isEnabled=True)
    assert x_info.value.status_code == 403


@pytest.fixture
def one(server_factory, test_system_settings):
    return server_factory('one', setup_settings=test_system_settings)

@pytest.fixture
def two(server_factory):
    return server_factory('two')


def test_merge_take_local_settings(one, two, test_system_settings):
    # Start two servers without predefined systemName
    # and move them to working state
    check_system_settings(one, **test_system_settings['systemSettings'])

    # On each server update some globalSettings to different values
    expected_arecontRtspEnabled = change_bool_setting(one, 'arecontRtspEnabled')
    expected_auditTrailEnabled = not change_bool_setting(two, 'auditTrailEnabled')

    # Merge systems (takeRemoteSettings = false)
    two.merge_systems(one)
    wait_for_settings_merge(one, two)
    check_system_settings(
      one,
      arecontRtspEnabled=bool_to_str(expected_arecontRtspEnabled),
      auditTrailEnabled=bool_to_str(expected_auditTrailEnabled))

    # Ensure both servers are merged and sync
    expected_arecontRtspEnabled = not change_bool_setting(one, 'arecontRtspEnabled')
    wait_for_settings_merge(one, two)
    check_system_settings(
        two,
        arecontRtspEnabled=bool_to_str(expected_arecontRtspEnabled))


# Start two servers without predefined systemName
# and move them to working state
def test_merge_take_remote_settings(one, two):
    # On each server update some globalSettings to different values
    expected_arecontRtspEnabled = not change_bool_setting(one, 'arecontRtspEnabled')
    expected_auditTrailEnabled = not change_bool_setting(two, 'auditTrailEnabled')

    # Merge systems (takeRemoteSettings = true)
    two.merge_systems(one, take_remote_settings=True)
    wait_for_settings_merge(one, two)
    check_system_settings(
      one,
      arecontRtspEnabled=bool_to_str(expected_arecontRtspEnabled),
      auditTrailEnabled=bool_to_str(expected_auditTrailEnabled))

    # Ensure both servers are merged and sync
    expected_auditTrailEnabled = not change_bool_setting(one, 'auditTrailEnabled')
    wait_for_settings_merge(one, two)
    check_system_settings(
        two,
        auditTrailEnabled=bool_to_str(expected_auditTrailEnabled))


def test_merge_cloud_with_local(server_factory, cloud_account, test_system_settings):
    # Start local server systemName
    # and move it to working state
    one = server_factory('one', setup_cloud_account=cloud_account, setup_settings=test_system_settings)
    two = server_factory('two')

    # Merge systems (takeRemoteSettings = False) -> Error
    with pytest.raises(HttpError) as x_info:
        two.merge_systems(one)
    assert x_info.value.reason == 'DEPENDENT_SYSTEM_BOUND_TO_CLOUD'

    # Merge systems (takeRemoteSettings = true)
    two.merge_systems(one, take_remote_settings=True)
    wait_for_settings_merge(one, two)
    check_system_settings(
        two, **test_system_settings['systemSettings'])


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/71467018/Merge+systems+test#Mergesystemstest-test_merge_cloud_systems
def test_merge_cloud_systems(server_factory, cloud_account_factory):
    cloud_account_1 = cloud_account_factory()
    cloud_account_2 = cloud_account_factory()
    one = server_factory('one', setup_cloud_account=cloud_account_1)
    two = server_factory('two', setup_cloud_account=cloud_account_2)

    # Merge 2 cloud systems one way
    with pytest.raises(HttpError) as x_info:
        two.merge_systems(one, take_remote_settings=False)
    assert x_info.value.reason == 'BOTH_SYSTEM_BOUND_TO_CLOUD'

    # Merge 2 cloud systems the other way
    with pytest.raises(HttpError) as x_info:
        two.merge_systems(one, take_remote_settings=True)
    assert x_info.value.reason == 'BOTH_SYSTEM_BOUND_TO_CLOUD'

    # Test default (admin) user disabled
    check_admin_disabled(one)


def test_cloud_merge_after_disconnect(server_factory, cloud_account, test_system_settings):
    # Setup cloud and wait new cloud credentials
    one = server_factory('one', setup_cloud_account=cloud_account, setup_settings=test_system_settings)
    two = server_factory('two', setup_cloud_account=cloud_account)

    # Check setupCloud's settings on Server1
    check_system_settings(
        one, **test_system_settings['systemSettings'])

    # Disconnect Server2 from cloud
    new_password = 'new_password'
    two.detach_from_cloud(new_password)

    # Merge systems (takeRemoteSettings = true)
    two.merge_systems(one, take_remote_settings=True)
    wait_for_settings_merge(one, two)

    # Ensure both servers are merged and sync
    expected_auditTrailEnabled = not change_bool_setting(one, 'auditTrailEnabled')
    wait_for_settings_merge(one, two)
    check_system_settings(
        two,
        auditTrailEnabled=bool_to_str(expected_auditTrailEnabled))


def wait_entity_merge_done(one, two, method, api_object, api_method, expected_resources):
    start_time = datetime_utc_now()
    while True:
        result_1 = one.rest_api.get_api_fn(method, api_object, api_method)()
        result_2 = two.rest_api.get_api_fn(method, api_object, api_method)()
        if result_1 == result_2:
            got_resources = [v['id'] for v in result_1 if v['id'] in expected_resources]
            assert got_resources == expected_resources
            return
        if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
            assert result_1 == result_2
        time.sleep(MEDIASERVER_MERGE_TIMEOUT.total_seconds() / 10.0)

def test_merge_resources(server_factory):
    one = server_factory('one')
    two = server_factory('two')
    user_data = generator.generate_user_data(1)
    camera_data = generator.generate_camera_data(1)
    one.rest_api.ec2.saveUser.POST(**user_data)
    two.rest_api.ec2.saveCamera.POST(**camera_data)
    two.merge_systems(one)
    wait_entity_merge_done(one, two, 'GET', 'ec2', 'getUsers', [user_data['id']])
    wait_entity_merge_done(one, two, 'GET', 'ec2', 'getCamerasEx', [camera_data['id']])


def test_restart_one_server(server_factory, cloud_account):
    one = server_factory('one')
    two = server_factory('two')
    one.merge([two])

    # Stop Server2 and clear its database
    guid2 = two.ecs_guid
    two.reset()

    # Remove Server2 from database on Server1
    one.rest_api.ec2.removeResource.POST(id=guid2)

    # Start server 2 again and move it from initial to working state
    two.setup_cloud_system(cloud_account)
    two.rest_api.ec2.getUsers.GET()

    # Merge systems (takeRemoteSettings = false)
    two.merge_systems(one)
    two.rest_api.ec2.getUsers.GET()

    # Ensure both servers are merged and sync
    expected_arecontRtspEnabled = not change_bool_setting(one, 'arecontRtspEnabled')
    wait_for_settings_merge(one, two)
    check_system_settings(
        two,
        arecontRtspEnabled=bool_to_str(expected_arecontRtspEnabled))
