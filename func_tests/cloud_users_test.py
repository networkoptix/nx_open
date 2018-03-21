"""Mediaserver and cloud protocol synchronization

Initial task https://networkoptix.atlassian.net/browse/UT-42.
"""
import pytest

from test_utils.utils import wait_until

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
    server = server_factory.create('server', setup=False)
    setup_cloud_system(server, cloud_account, {})
    server.rest_api.ec2.saveUser.POST(
        email=SECOND_CLOUD_USER,
        isCloud=True,
        permissions=ADMIN_PERMISSIONS)

    users = server.rest_api.ec2.getUsers.GET()
    second_cloud_users = [u for u in users if u['name'] == SECOND_CLOUD_USER]
    assert len(second_cloud_users) == 1  # One second cloud user is expected
    assert second_cloud_users[0]['isEnabled']
    assert second_cloud_users[0]['isCloud']

    server.rest_api = server.rest_api.with_credentials(SECOND_CLOUD_USER, SECOND_CLOUD_PASSWORD)
    wait_until(server.rest_api.credentials_work)
