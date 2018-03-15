"""Merge test

Initial task https://networkoptix.atlassian.net/browse/TEST-177
"""

import logging
import time

import pytest
import yaml
from pathlib2 import Path

import server_api_data_generators as generator
from test_utils.api_shortcuts import get_server_id, get_system_settings
from test_utils.networking import setup_networks
from test_utils.rest_api import HttpError, RestApiError
from test_utils.server import MEDIASERVER_MERGE_TIMEOUT
from test_utils.utils import bool_to_str, datetime_utc_now, str_to_bool, wait_until

log = logging.getLogger(__name__)


@pytest.fixture
def test_system_settings():
    return dict(systemSettings=dict(
        cameraSettingsOptimization=bool_to_str(False),
        autoDiscoveryEnabled=bool_to_str(False),
        statisticsAllowed=bool_to_str(False)))


def check_system_settings(server, **kw):
    settings_to_check = {k: v for k, v in get_system_settings(server.rest_api).items() if k in kw.keys()}
    assert settings_to_check == kw


def change_bool_setting(server, setting):
    val = str_to_bool(get_system_settings(server.rest_api)[setting])
    settings = {setting:  bool_to_str(not val)}
    server.rest_api.get('/api/systemSettings', params=settings)
    check_system_settings(server, **settings)
    return val


def wait_for_settings_merge(one, two):
    assert wait_until(
        lambda: get_system_settings(one.rest_api) == get_system_settings(two.rest_api),
        name='for same response to /api/systemSettings from {} and {}'.format(one.machine.alias, two.machine.alias))


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


@pytest.fixture()
def vms(vm_pools, hypervisor):
    path = Path(__file__).with_name('network_layouts') / 'direct-no_merge.yaml'
    layout = yaml.load(path.read_text())
    vms, _ = setup_networks(vm_pools, hypervisor, layout['networks'], {})
    return vms


@pytest.fixture
def one(vms, server_factory, test_system_settings):
    return server_factory.create('one', vm=vms['first'], setup_settings=test_system_settings)


@pytest.fixture
def two(vms, server_factory):
    return server_factory.create('two', vm=vms['second'])


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


def test_merge_cloud_with_local(vms, server_factory, cloud_account, test_system_settings):
    # Start local server systemName
    # and move it to working state
    one = server_factory.create('one', vm=vms['first'], setup_cloud_account=cloud_account, setup_settings=test_system_settings)
    two = server_factory.create('two', vm=vms['second'])

    # Merge systems (takeRemoteSettings = False) -> Error
    try:
        two.merge_systems(one)
    except RestApiError as e:
        assert e.error_string == 'DEPENDENT_SYSTEM_BOUND_TO_CLOUD'
    else:
        assert False, 'Expected: DEPENDENT_SYSTEM_BOUND_TO_CLOUD'

    # Merge systems (takeRemoteSettings = true)
    two.merge_systems(one, take_remote_settings=True)
    wait_for_settings_merge(one, two)
    check_system_settings(
        two, **test_system_settings['systemSettings'])


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/71467018/Merge+systems+test#Mergesystemstest-test_merge_cloud_systems
def test_merge_cloud_systems(vms, server_factory, cloud_account_factory):
    cloud_account_1 = cloud_account_factory()
    cloud_account_2 = cloud_account_factory()
    one = server_factory.create('one', vm=vms['first'], setup_cloud_account=cloud_account_1)
    two = server_factory.create('two', vm=vms['second'], setup_cloud_account=cloud_account_2)

    # Merge 2 cloud systems one way
    try:
        two.merge_systems(one, take_remote_settings=False)
    except RestApiError as e:
        assert e.error_string == 'BOTH_SYSTEM_BOUND_TO_CLOUD'
    else:
        assert False, 'Expected: BOTH_SYSTEM_BOUND_TO_CLOUD'

    # Merge 2 cloud systems the other way
    try:
        two.merge_systems(one, take_remote_settings=True)
    except RestApiError as e:
        assert e.error_string == 'BOTH_SYSTEM_BOUND_TO_CLOUD'
    else:
        assert False, 'Expected: BOTH_SYSTEM_BOUND_TO_CLOUD'

    # Test default (admin) user disabled
    check_admin_disabled(one)


def test_cloud_merge_after_disconnect(vms, server_factory, cloud_account, test_system_settings):
    # Setup cloud and wait new cloud credentials
    one = server_factory.create(
        'one', vm=vms['first'],
        setup_cloud_account=cloud_account, setup_settings=test_system_settings)
    two = server_factory.create('two', vm=vms['second'], setup_cloud_account=cloud_account)

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


def test_merge_resources(vms, server_factory):
    one = server_factory.create('one', vm=vms['first'])
    two = server_factory.create('two', vm=vms['second'])
    user_data = generator.generate_user_data(1)
    camera_data = generator.generate_camera_data(1)
    one.rest_api.ec2.saveUser.POST(**user_data)
    two.rest_api.ec2.saveCamera.POST(**camera_data)
    two.merge_systems(one)
    wait_entity_merge_done(one, two, 'GET', 'ec2', 'getUsers', [user_data['id']])
    wait_entity_merge_done(one, two, 'GET', 'ec2', 'getCamerasEx', [camera_data['id']])


def test_restart_one_server(vms, server_factory, cloud_account):
    one = server_factory.create('one', vm=vms['first'])
    two = server_factory.create('two', vm=vms['second'])
    one.merge_systems(two)

    # Stop Server2 and clear its database
    guid2 = get_server_id(two.rest_api)
    two.stop()
    two.installation.cleanup_var_dir()
    two.start()

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
