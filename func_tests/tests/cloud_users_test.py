"""Mediaserver and cloud protocol synchronization

Initial task https://networkoptix.atlassian.net/browse/UT-42.
"""
import pytest

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
def test_mediaserver_cloud_protocol_synchronization(one_mediaserver, cloud_account, cloud_host):
    one_mediaserver.installation.set_cloud_host(cloud_host)
    one_mediaserver.os_access.networking.enable_internet()
    one_mediaserver.start()
    one_mediaserver.api.setup_cloud_system(cloud_account)
    one_mediaserver.api.generic.post('ec2/saveUser', dict(
        email=SECOND_CLOUD_USER,
        isCloud=True,
        permissions=ADMIN_PERMISSIONS))

    users = one_mediaserver.api.generic.get('ec2/getUsers')
    second_cloud_users = [u for u in users if u['name'] == SECOND_CLOUD_USER]
    assert len(second_cloud_users) == 1  # One second cloud user is expected
    assert second_cloud_users[0]['isEnabled']
    assert second_cloud_users[0]['isCloud']

    one_mediaserver.api.generic.http.set_credentials(SECOND_CLOUD_USER, SECOND_CLOUD_PASSWORD)
    wait_for_true(one_mediaserver.api.credentials_work)
    assert not one_mediaserver.installation.list_core_dumps()
