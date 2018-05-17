"""Mediaserver database test

https://networkoptix.atlassian.net/wiki/spaces/SD/pages/85690455/Mediaserver+database+test

It tests some cases with main server database, such as:
- compatibility with database from previous version;
- backup/restore.

You have to prepare old-version database files for the test in the bin directory:
* for 4.1 - v2.4.1-box1.db, v2.4.1-box2.db

All necessary files are on the rsync://noptix.enk.me/buildenv/test
"""

import time

import pytest

from framework.installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from framework.merging import merge_systems, setup_local_system
from framework.os_access.path import copy_file
from framework.utils import SimpleNamespace, bool_to_str, datetime_utc_now

SERVER_CONFIG = dict(
    one=SimpleNamespace(
        DATABASE_FILE_V_2_4='v2.4.1-box1.db',
        CAMERA_GUID='{ed93120e-0f50-3cdf-39c8-dd52a640688c}',
        SERVER_GUID='{62a54ada-e7c7-0d09-c41a-4ab5c1251db8}',
        ),
    two=SimpleNamespace(
        DATABASE_FILE_V_2_4='v2.4.1-box2.db',
        CAMERA_GUID='{a6b88c1b-92c3-0c27-b5a1-76a1246fd9ed}',
        SERVER_GUID='{88b807ab-0a0f-800e-e2c3-b640b31f3a1c}',
        ),
    )

MEDIASERVER_DATABASE_PATH = 'var/ecs.sqlite'


@pytest.fixture(params=['current', '2.4'])
def db_version(request):
    return request.param


@pytest.fixture
def one(two_stopped_mediaservers, bin_dir, db_version):
    one, _ = two_stopped_mediaservers
    yield server('one', one, bin_dir, db_version)


@pytest.fixture
def two(two_stopped_mediaservers, bin_dir, db_version):
    _, two = two_stopped_mediaservers
    yield server('two', two, bin_dir, db_version)


def server(name, mediaserver, bin_dir, db_version):
    server_config = SERVER_CONFIG[name]
    if db_version == '2.4':
        config_file_params = dict(
            guidIsHWID='no',
            serverGuid=server_config.SERVER_GUID,
            minStorageSpace=1024*1024,  # 1M
        )
        mediaserver.installation.update_mediaserver_conf(config_file_params)
        copy_database_file(mediaserver, bin_dir, server_config.DATABASE_FILE_V_2_4)
    mediaserver.start()
    system_settings = dict(autoDiscoveryEnabled=bool_to_str(False))
    setup_local_system(mediaserver, system_settings)
    mediaserver.api.api.systemSettings.GET(statisticsAllowed=False)
    if db_version == '2.4':
        check_camera(mediaserver, server_config.CAMERA_GUID)
    return mediaserver


def copy_database_file(server, bin_dir, backup_db_filename):
    backup_db_path = bin_dir / backup_db_filename
    assert backup_db_path.exists(), (
        "Binary artifact required for this test (database file) '%s' does not exist." % backup_db_path)
    server_db_path = server.installation.dir / MEDIASERVER_DATABASE_PATH
    copy_file(backup_db_path, server.machine.Path(server_db_path))


def check_camera(server, camera_guid):
    cameras = [c for c in server.api.ec2.getCameras.GET() if c['id'] == camera_guid]
    assert len(cameras) == 1, "'%r': one of cameras '%s' is absent" % (server, camera_guid)


def assert_jsons_are_equal(json_one, json_two, json_name):
    """It fails after the first error"""
    if isinstance(json_one, dict):
        assert json_one.keys() == json_two.keys(), "'%s' {}s have different keys" % json_name
        for key in json_one.keys():
            entity_name = '%s.%s' % (json_name, key)
            assert_jsons_are_equal(json_one[key], json_two[key], entity_name)
    elif isinstance(json_one, list):
        assert len(json_one) == len(json_two), "'%s' lists have different lengths" % json_name
        for i, v in enumerate(json_one):
            entity_name = '%s[%d]' % (json_name, i)
            assert_jsons_are_equal(json_one[i], json_two[i], entity_name)
    else:
        assert json_one == json_two, json_name


def wait_until_servers_have_same_full_info(one, two):
    start_time = datetime_utc_now()
    while True:
        full_info_one = one.api.ec2.getFullInfo.GET()
        full_info_two = two.api.ec2.getFullInfo.GET()
        if full_info_one == full_info_two:
            return full_info_one
        if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
            assert full_info_one == full_info_two
        time.sleep(MEDIASERVER_MERGE_TIMEOUT.total_seconds() / 10.)


def wait_for_camera_disappearance_after_backup(server, camera_guid):
    start_time = datetime_utc_now()
    while True:
        cameras = [c for c in server.api.ec2.getCameras.GET()
                   if c['id'] == camera_guid]
        if not cameras:
            return
        if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
            pytest.fail('Camera %s did not disappear in %s after backup' % (
                camera_guid, MEDIASERVER_MERGE_TIMEOUT))
        time.sleep(MEDIASERVER_MERGE_TIMEOUT.total_seconds() / 10.)


# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/85690455/Mediaserver+database+test#Mediaserverdatabasetest-test_backup_restore
def test_backup_restore(artifact_factory, one, two, camera):
    merge_systems(two, one)
    full_info_initial = wait_until_servers_have_same_full_info(one, two)
    backup = one.api.ec2.dumpDatabase.GET()
    camera_guid = two.add_camera(camera)
    full_info_with_new_camera = wait_until_servers_have_same_full_info(one, two)
    assert full_info_with_new_camera != full_info_initial, (
        "ec2/getFullInfo data before and after saveCamera are the same")
    one.api.ec2.restoreDatabase.POST(data=backup['data'])
    wait_for_camera_disappearance_after_backup(one, camera_guid)
    full_info_after_backup_restore = wait_until_servers_have_same_full_info(one, two)
    try:
        assert full_info_after_backup_restore == full_info_initial
    except AssertionError:
        artifact_factory(['full_info_initial'],
                         name='full_info_initial').save_as_json(full_info_initial)
        artifact_factory(['full_info_after_backup_restore'],
                         name='full_info_after_backup_restore').save_as_json(full_info_after_backup_restore)
        raise

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


# To detect VMS-5969
# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/85690455/Mediaserver+database+test#Mediaserverdatabasetest-test_server_guids_changed
@pytest.mark.skip(reason="VMS-5969")
@pytest.mark.parametrize('db_version', ['current'])
def test_server_guids_changed(one, two):
    one.stop()
    two.stop()
    # To make server database and configuration file guids different
    one.installation.update_mediaserver_conf({'guidIsHWID': 'no', 'serverGuid': SERVER_CONFIG['one'].SERVER_GUID})
    two.installation.update_mediaserver_conf({'guidIsHWID': 'no', 'serverGuid': SERVER_CONFIG['two'].SERVER_GUID})
    one.start()
    two.start()
    setup_local_system(one, {})
    setup_local_system(two, {})
    merge_systems(two, one)
    wait_until_servers_have_same_full_info(one, two)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()
