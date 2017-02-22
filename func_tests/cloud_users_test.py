# $Id$
# Artem V. Nikitin
# https://networkoptix.atlassian.net/browse/UT-42
# Check mediaserver and cloud protocol synchronization

import pytest

CLOUD_USER_SECOND='vfedorov@networkoptix.com'
CLOUD_PASSWORD_SECOND='123qweasd'
ADMIN_PERMISSIONS=("GlobalAdminPermission|GlobalEditCamerasPermission|" 
                   "GlobalControlVideoWallPermission|GlobalViewLogsPermission|"
                   "GlobalViewArchivePermission|GlobalExportPermission|"
                   "GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|"
                   "GlobalUserInputPermission|GlobalAccessAllMediaPermission")

@pytest.fixture
def env(env_builder, server):
    server = server(setup=False)
    return env_builder(server=server)

def test_mediaserver_cloud_protocol_syncronization(env):
    env.server.setup_cloud_system()
    env.server.rest_api.ec2.saveUser.post(
        email = 'vfedorov@networkoptix.com',
        isCloud = True,
        permissions=ADMIN_PERMISSIONS)

    users = env.server.rest_api.ec2.getUsers.get()
    second_cloud_users = [u for u in users if u['name'] == CLOUD_USER_SECOND]
    assert len(second_cloud_users) == 1  # One second cloud user is expected
    assert second_cloud_users[0]['isEnabled']
    assert second_cloud_users[0]['isCloud']

    env.server.set_user_password(CLOUD_USER_SECOND, CLOUD_PASSWORD_SECOND)

    
        
    
