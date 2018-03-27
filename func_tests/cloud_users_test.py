"""Mediaserver and cloud protocol synchronization

Initial task https://networkoptix.atlassian.net/browse/UT-42.
"""
import pytest

from test_utils.merging import setup_cloud_system
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
def test_mediaserver_cloud_protocol_synchronization(running_linux_server, cloud_account):
    setup_cloud_system(running_linux_server, cloud_account, {})
    running_linux_server.api.ec2.saveUser.POST(
        email=SECOND_CLOUD_USER,
        isCloud=True,
        permissions=ADMIN_PERMISSIONS)

    users = running_linux_server.api.ec2.getUsers.GET()
    second_cloud_users = [u for u in users if u['name'] == SECOND_CLOUD_USER]
    assert len(second_cloud_users) == 1  # One second cloud user is expected
    assert second_cloud_users[0]['isEnabled']
    assert second_cloud_users[0]['isCloud']

    running_linux_server.api = running_linux_server.api.with_credentials(SECOND_CLOUD_USER, SECOND_CLOUD_PASSWORD)
    wait_until(running_linux_server.api.credentials_work)
    assert not running_linux_server.installation.list_core_dumps()
