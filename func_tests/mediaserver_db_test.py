'''Mediaserver database test

   https://networkoptix.atlassian.net/wiki/spaces/SD/pages/85690455/Mediaserver+database+test

   It tests some cases with main server database, such as:
   - compatibility with database from previous version;
   - backup/restore.

   You have to prepare old-version database files for the test in the bin directory:
   * for 4.1 - v2.4.1-box1.db, v2.4.1-box2.db

   All necessary files are on the rsync://noptix.enk.me/buildenv/test
'''

import abc
import logging
import pytest
import os
import time
import json
from test_utils.server import MEDIASERVER_MERGE_TIMEOUT_SEC
import server_api_data_generators as generator

BOX_1_DATABASE_FILE_V_2_4 = 'v2.4.1-box1.db'
BOX_2_DATABASE_FILE_V_2_4 = 'v2.4.1-box2.db'
BOX_1_CAMERA_GUID = '{ed93120e-0f50-3cdf-39c8-dd52a640688c}'
BOX_2_CAMERA_GUID = '{a6b88c1b-92c3-0c27-b5a1-76a1246fd9ed}'
BOX_1_SERVER_GUID = '{62a54ada-e7c7-0d09-c41a-4ab5c1251db8}'
BOX_2_SERVER_GUID = '{88b807ab-0a0f-800e-e2c3-b640b31f3a1c}'
MEDIASERVER_DATABASE_PATH = 'var/ecs.sqlite'


@pytest.fixture(params=['current', '2.4'])
def mediaserver_version(request):
    return request.param


@pytest.fixture
def env(env_builder, server, run_options, mediaserver_version):
    if mediaserver_version == '2.4':
        built_env = env_2_4_version(env_builder, server, run_options)
    else:
        built_env = env_current_version(env_builder, server)
    built_env.one.start_service()
    built_env.two.start_service()
    system_settings = dict(autoDiscoveryEnabled=False)
    built_env.one.setup_local_system(systemSettings=system_settings)
    built_env.two.setup_local_system(systemSettings=system_settings)
    built_env.one.set_system_settings(statisticsAllowed=False)
    built_env.two.set_system_settings(statisticsAllowed=False)
    if mediaserver_version == '2.4':
        check_camera(built_env.one, BOX_1_CAMERA_GUID)
        check_camera(built_env.two, BOX_2_CAMERA_GUID)
    return built_env


def copy_database_file(server, bin_dir, backup_db_filename):
    backup_db_path = os.path.abspath(os.path.join(bin_dir, backup_db_filename))
    assert os.path.exists(backup_db_path), (
        "Binary artifact required for this test (database file) '%s' does not exist." % backup_db_path)
    server_db_path = os.path.join(server.dir, MEDIASERVER_DATABASE_PATH)
    server.box.host.put_file(backup_db_path, server_db_path)


def check_camera(server, camera_guid):
    cameras = [c for c in server.rest_api.ec2.getCameras.GET() if c['id'] == camera_guid]
    assert len(cameras) == 1, "'%r': one of cameras '%s' is absent" % (server, camera_guid)


def env_current_version(env_builder, server):
    one = server(start=False)
    two = server(start=False)
    return env_builder(one=one, two=two)


def env_2_4_version(env_builder, server, run_options):
    config_file_params_1 = dict(guidIsHWID='no', serverGuid=BOX_1_SERVER_GUID)
    config_file_params_2 = dict(guidIsHWID='no', serverGuid=BOX_2_SERVER_GUID)
    one = server(start=False, config_file_params=config_file_params_1)
    two = server(start=False, config_file_params=config_file_params_2)
    env = env_builder(one=one, two=two)
    copy_database_file(env.one, run_options.bin_dir, BOX_1_DATABASE_FILE_V_2_4)
    copy_database_file(env.two, run_options.bin_dir, BOX_2_DATABASE_FILE_V_2_4)
    return env


def assert_jsons_are_equal(json_one, json_two, json_name):
    '''It fails after the first error'''
    if isinstance(json_one, dict):
        assert json_one.keys() == json_two.keys(), "'%s' dicts have different keys" % json_name
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


def store_json_data(filepath, json_data):
    with open(filepath, 'wb') as f:
        f.write(json.dumps(json_data, sort_keys=True, indent=4, separators=(',', ': ')))


def wait_for_merge_and_get_full_info(env):
    start = time.time()
    while True:
        full_info_one = env.one.rest_api.ec2.getFullInfo.GET()
        full_info_two = env.two.rest_api.ec2.getFullInfo.GET()
        if full_info_one == full_info_two:
            return full_info_one
        if time.time() - start >= MEDIASERVER_MERGE_TIMEOUT_SEC:
            assert full_info_one == full_info_two
        time.sleep(MEDIASERVER_MERGE_TIMEOUT_SEC / 10.)


def wait_camera_disappearance_after_backup(server, camera_guid):
    start = time.time()
    while True:
        cameras = [c for c in server.rest_api.ec2.getCameras.GET()
                   if c['id'] == camera_guid]
        if len(cameras) == 0:
            return
        if time.time() - start >= MEDIASERVER_MERGE_TIMEOUT_SEC:
            assert len(cameras) == 0, "'%r' unexpected camera '%s' after backup" % (
                server, camera_guid)
        time.sleep(MEDIASERVER_MERGE_TIMEOUT_SEC / 10.)


def test_backup_restore(env, camera):
    env.two.merge_systems(env.one)
    full_info_initial = wait_for_merge_and_get_full_info(env)
    store_json_data('full_info_initial', full_info_initial)
    backup = env.one.rest_api.ec2.dumpDatabase.GET()
    camera_guid = env.two.add_camera(camera)
    full_info_with_new_camera = wait_for_merge_and_get_full_info(env)
    assert full_info_with_new_camera != full_info_initial, (
        "ec2/getFullInfo data before and after saveCamera are the same")
    env.one.rest_api.ec2.restoreDatabase.POST(data=backup['data'])
    wait_camera_disappearance_after_backup(env.one, camera_guid)
    full_info_after_backup_restore = wait_for_merge_and_get_full_info(env)
    store_json_data('full_info_after_backup_restore', full_info_after_backup_restore)
    assert full_info_after_backup_restore == full_info_initial


# To detect VMS-5969
@pytest.mark.skip(reason="VMS-5969")
@pytest.mark.parametrize('mediaserver_version', ['current'])
def test_server_guids_changed(env):
    env.one.stop_service()
    env.two.stop_service()
    # To make server database and configuration file guids different
    env.one.change_config(guidIsHWID='no', serverGuid=BOX_1_SERVER_GUID)
    env.two.reset_config(guidIsHWID='no', serverGuid=BOX_2_SERVER_GUID)
    env.one.start_service()
    env.two.start_service()
    env.one.setup_local_system()
    env.two.setup_local_system()
    env.two.merge_systems(env.one)
    wait_for_merge_and_get_full_info(env)
