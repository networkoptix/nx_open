"""Mediaserver and cloud protocol synchronization

Initial task https://networkoptix.atlassian.net/browse/UT-42.
"""
import pytest

from framework.merging import setup_cloud_system
from framework.waiting import wait_for_true

SECOND_CLOUD_USER = 'vfedorov@networkoptix.com'
SECOND_CLOUD_PASSWORD = '123qweasd'
ADMIN_PERMISSIONS = '|'.join([
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
def test_mediaserver_cloud_protocol_synchronization(running_linux_mediaserver, cloud_account):
    setup_cloud_system(running_linux_mediaserver, cloud_account, {})
    running_linux_mediaserver.api.ec2.saveUser.POST(
        email=SECOND_CLOUD_USER,
        isCloud=True,
        permissions=ADMIN_PERMISSIONS)

    users = running_linux_mediaserver.api.ec2.getUsers.GET()
    second_cloud_users = [u for u in users if u['name'] == SECOND_CLOUD_USER]
    assert len(second_cloud_users) == 1  # One second cloud user is expected
    assert second_cloud_users[0]['isEnabled']
    assert second_cloud_users[0]['isCloud']

    running_linux_mediaserver.api = running_linux_mediaserver.api.with_credentials(
        SECOND_CLOUD_USER, SECOND_CLOUD_PASSWORD)
    wait_for_true(
        running_linux_mediaserver.api.credentials_work,
        "credentials of {} work".format(running_linux_mediaserver.api))
    assert not running_linux_mediaserver.installation.list_core_dumps()
