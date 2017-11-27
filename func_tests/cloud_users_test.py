'''Mediaserver and cloud protocol synchronization

   Initial task https://networkoptix.atlassian.net/browse/UT-42
'''
import pytest


SECOND_CLOUD_USER='vfedorov@networkoptix.com'
SECOND_CLOUD_PASSWORD='123qweasd'
ADMIN_PERMISSIONS='|'.join([
    'GlobalAdminPermission',
    'GlobalEditCamerasPermission', 
    'GlobalControlVideoWallPermission',
    'GlobalViewLogsPermission',
    'GlobalViewArchivePermission',
    'GlobalExportPermission'
    'GlobalViewBookmarksPermission',
    'GlobalManageBookmarksPermission',
    'GlobalUserInputPermission',
    'GlobalAccessAllMediaPermission'])

@pytest.mark.skip(reason="Disabled until release")
def test_mediaserver_cloud_protocol_synchronization(server_factory, cloud_account):
    server = server_factory('server', setup=False)
    server.setup_cloud_system(cloud_account)
    server.rest_api.ec2.saveUser.POST(
        email=SECOND_CLOUD_USER,
        isCloud=True,
        permissions=ADMIN_PERMISSIONS)

    users = server.rest_api.ec2.getUsers.GET()
    second_cloud_users = [u for u in users if u['name'] == SECOND_CLOUD_USER]
    assert len(second_cloud_users) == 1  # One second cloud user is expected
    assert second_cloud_users[0]['isEnabled']
    assert second_cloud_users[0]['isCloud']

    server.set_user_password(SECOND_CLOUD_USER, SECOND_CLOUD_PASSWORD)
