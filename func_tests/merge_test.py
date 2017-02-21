# $Id$
# Artem V. Nikitin
# Merge test (https://networkoptix.atlassian.net/browse/TEST-177)

import pytest, time
from test_utils import bool_to_str, str_to_bool
from server import MEDIASERVER_MERGE_TIMEOUT_SEC
from server_rest_api import ServerRestApiError, HttpError

@pytest.fixture
def env(env_builder, server):
    one = server(setup=False)
    two = server(setup=False)
    return env_builder(one=one, two=two)

def get_test_system_settings():
    return dict(systemSettings =
                dict(cameraSettingsOptimization = 'false',
                     autoDiscoveryEnabled = 'false',
                     statisticsAllowed = 'false'))

def check_system_settings(server, **kw):
    settings_to_check = {k: v for k, v in server.settings.iteritems() if kw.has_key(k) }
    assert settings_to_check == kw

def change_bool_setting(server, setting):
    val = str_to_bool(server.settings[setting])
    settings = { setting:  bool_to_str(not val) }
    server.set_system_system_settings(**settings)
    check_system_settings(server, **settings)
    return val

def wait_for_settings_merge(env):
    t = time.time()
    while time.time() - t < MEDIASERVER_MERGE_TIMEOUT_SEC:
        env.one.load_system_settings()
        env.two.load_system_settings()
        if env.one.settings == env.two.settings:
            return
        time.sleep(0.5)
    assert env.one.settings == env.two.settings

def check_admin_disabled(server):
    users = server.rest_api.ec2.getUsers.get()
    admin_users = [u for u in users if u['name'] == 'admin']
    assert len(admin_users) == 1  # One cloud user is expected
    assert not admin_users[0]['isEnabled']
    with pytest.raises(HttpError) as x_info:
        server.rest_api.ec2.saveUser.post(
            id = admin_users[0]['id'],
            isEnabled = True)
    assert x_info.value.status_code == 403

def test_merge_take_local_settings(env):
    # Start two servers without predefined systemName
    # and move them to working state
    settings = get_test_system_settings()
    env.one.setup_local_system(**settings)
    env.two.setup_local_system()
    check_system_settings(env.one, **settings['systemSettings'])

    # On each server update some globalSettings to different values
    exp_arecontRtspEnabled = \
        change_bool_setting(env.one, 'arecontRtspEnabled')
    exp_auditTrailEnabled = \
        not change_bool_setting(env.two, 'auditTrailEnabled')
    
    # Merge systems (takeRemoteSettings = false)
    env.two.merge_systems(env.one)
    wait_for_settings_merge(env)
    check_system_settings(
      env.one,
      arecontRtspEnabled = bool_to_str(exp_arecontRtspEnabled),
      auditTrailEnabled = bool_to_str(exp_auditTrailEnabled))

    # Ensure both servers are merged and sync
    exp_arecontRtspEnabled = \
        not change_bool_setting(env.one, 'arecontRtspEnabled')
    wait_for_settings_merge(env)
    check_system_settings(
        env.two,
        arecontRtspEnabled = bool_to_str(exp_arecontRtspEnabled))

def test_merge_take_remote_settings(env):
    # Start two servers without predefined systemName
    # and move them to working state
    settings = get_test_system_settings()
    env.one.setup_local_system(**settings)
    env.two.setup_local_system()

    # On each server update some globalSettings to different values
    exp_arecontRtspEnabled = \
        not change_bool_setting(env.one, 'arecontRtspEnabled')
    exp_auditTrailEnabled = \
        not change_bool_setting(env.two, 'auditTrailEnabled')

    # Merge systems (takeRemoteSettings = true)
    env.two.merge_systems(env.one, take_remote_settings=True)
    wait_for_settings_merge(env)
    check_system_settings(
      env.one,
      arecontRtspEnabled = bool_to_str(exp_arecontRtspEnabled),
      auditTrailEnabled = bool_to_str(exp_auditTrailEnabled))

    # Ensure both servers are merged and sync
    exp_auditTrailEnabled = \
        not change_bool_setting(env.one, 'auditTrailEnabled')
    wait_for_settings_merge(env)
    check_system_settings(
        env.two,
        auditTrailEnabled = bool_to_str(exp_auditTrailEnabled))

def test_merge_cloud_with_local(env):
    # Start local server systemName
    # and move it to working state
    env.two.setup_local_system()
    
    # Setup cloud and wait new cloud credentials
    settings = get_test_system_settings()
    env.one.setup_cloud_system(**settings)

    # Merge systems (takeRemoteSettings = False) -> Error
    with pytest.raises(ServerRestApiError) as x_info:
        env.two.merge_systems(env.one)
    assert x_info.value.error_string == 'DEPENDENT_SYSTEM_BOUND_TO_CLOUD'

    # Merge systems (takeRemoteSettings = true)
    env.two.merge_systems(env.one, take_remote_settings=True)
    wait_for_settings_merge(env)
    check_system_settings(
        env.two, **settings['systemSettings'])


def test_merge_cloud_systems(env):
    env.one.setup_cloud_system()
    env.two.setup_cloud_system()
    
    # Merge 2 cloud systems (takeRemoteSettings = False) -> Error
    with pytest.raises(ServerRestApiError) as x_info:
        env.two.merge_systems(env.one)
    assert x_info.value.error_string == 'BOTH_SYSTEM_BOUND_TO_CLOUD'

    with pytest.raises(ServerRestApiError) as x_info:
        env.two.merge_systems(env.one, take_remote_settings=True)
    assert x_info.value.error_string == 'BOTH_SYSTEM_BOUND_TO_CLOUD'

    # Test default (admin) user disabled
    check_admin_disabled(env.one)

def test_cloud_merge_after_disconnect(env):
    # Setup cloud and wait new cloud credentials
    settings = get_test_system_settings()
    env.one.setup_cloud_system(**settings)
    env.two.setup_cloud_system()

    # Check setupCloud's settings on Server1
    check_system_settings(
        env.one, **settings['systemSettings'])

    # Disconnect Server2 from cloud
    new_password = 'new_password'
    env.two.detach_from_cloud(new_password)

    # Merge systems (takeRemoteSettings = true)
    env.two.merge_systems(env.one, take_remote_settings=True)
    wait_for_settings_merge(env)
    
    # Ensure both servers are merged and sync
    exp_auditTrailEnabled = \
        not change_bool_setting(env.one, 'auditTrailEnabled')
    wait_for_settings_merge(env)
    check_system_settings(
        env.two,
        auditTrailEnabled = bool_to_str(exp_auditTrailEnabled))

@pytest.fixture
def env_merged(env_builder, server):
    one = server()
    two = server()
    return env_builder(merge_servers=[one, two], one=one, two=two)

def test_restart_one_server(env_merged):
    # Stop Server2 and clear its database
    guid2 = env_merged.two.ecs_guid
    env_merged.two.reset()
   
    # Remove Server2 from database on Server1
    env_merged.one.rest_api.ec2.removeResource.post(id = guid2)

    # Start server 2 again and move it from initial to working state
    env_merged.two.setup_cloud_system()
    env_merged.two.rest_api.ec2.getUsers.get()

    # Merge systems (takeRemoteSettings = false)
    env_merged.two.merge_systems(env_merged.one)
    env_merged.two.rest_api.ec2.getUsers.get()

    # Ensure both servers are merged and sync
    exp_arecontRtspEnabled = \
         not change_bool_setting(env_merged.one, 'arecontRtspEnabled')
    wait_for_settings_merge(env_merged)
    check_system_settings(
        env_merged.two,
        arecontRtspEnabled = bool_to_str(exp_arecontRtspEnabled))
