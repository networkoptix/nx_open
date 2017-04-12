'''Mediaserver database test

   It tests some cases with main server database, such as:
   - compatibility with database from previous version;
   - backup/restore.

   You have to prepare old-version database files for the test in the bin directory:
   * for 4.1 - v2.4.1-box1.db, v2.4.1-box2.db

   All necessary files are on the rsync://noptix.enk.me/buildenv/test
'''

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


class DatabaseInfo(object):

    def __init__(self, filename, server_guid, camera_guid):
        self.filename = filename
        self.server_guid = server_guid
        self.camera_guid = camera_guid


class DatabaseCurrent(object):

    def prepare_environment(self, env_builder, server):
        '''Just start servers as usual'''
        one = server(start=False)
        two = server(start=False)
        return env_builder(one=one, two=two)

    def check_resources(self, env):
        '''Do nothing for current version'''
        pass


class DatabasePrevious(DatabaseCurrent):

    def __init__(self, bin_dir, database_1, database_2):
        self.bin_dir = bin_dir
        self.database_1 = database_1
        self.database_2 = database_2

    def prepare_environment(self, env_builder, server):
        '''Start servers with old-version databases'''
        config_file_params_1 = dict(removeDbOnStartup=0, guidIsHWID='no',
                                    serverGuid=self.database_1.server_guid)
        config_file_params_2 = dict(removeDbOnStartup=0, guidIsHWID='no',
                                    serverGuid=self.database_2.server_guid)
        one = server(start=False, config_file_params=config_file_params_1)
        two = server(start=False, config_file_params=config_file_params_2)
        env = env_builder(one=one, two=two)
        # Copy old-version database files to boxes
        self._copy_database_file(env.one, self.database_1.filename)
        self._copy_database_file(env.two, self.database_2.filename)
        return env

    def check_resources(self, env):
        '''Check database resources (cameras, etc..) have been loaded to servers'''
        self._check_camera(env.one, self.database_1.camera_guid)
        self._check_camera(env.two, self.database_2.camera_guid)

    def _copy_database_file(self, server, backup_db_filename):
        backup_db_path = os.path.abspath(os.path.join(self.bin_dir, backup_db_filename))
        assert os.path.exists(backup_db_path), (
            "Database file '%s' for '%r' is not exists" % (backup_db_path, server.box))
        server_db_path = os.path.join(server.dir, MEDIASERVER_DATABASE_PATH)
        with open(backup_db_path, 'rb') as f:
            server.box.host.write_file(server_db_path, f.read())

    def _check_camera(self, server, camera_guid):
        cameras = [c for c in server.rest_api.ec2.getCameras.GET() if c['id'] == camera_guid]
        assert len(cameras) == 1, "'%r': one of cameras '%s' is absent" % (
            server.box, camera_guid)


@pytest.fixture(params=['current', '2.4'])
def version(request):
    return request.param


@pytest.fixture
def db_version(run_options, version):
    if version == '2.4':
        return DatabasePrevious(
            run_options.bin_dir,
            DatabaseInfo(BOX_1_DATABASE_FILE_V_2_4, BOX_1_SERVER_GUID, BOX_1_CAMERA_GUID),
            DatabaseInfo(BOX_2_DATABASE_FILE_V_2_4, BOX_2_SERVER_GUID, BOX_2_CAMERA_GUID))
    return DatabaseCurrent()


@pytest.fixture
def env(env_builder, server, db_version):
    built_env = db_version.prepare_environment(env_builder, server)
    built_env.one.start_service()
    built_env.two.start_service()
    built_env.one.setup_local_system()
    built_env.two.setup_local_system()
    built_env.one.set_system_settings(
        autoDiscoveryEnabled=False,
        statisticsAllowed=False)
    built_env.two.set_system_settings(
        autoDiscoveryEnabled=False,
        statisticsAllowed=False)
    db_version.check_resources(built_env)
    return built_env


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
            assert len(cameras) == 0, "'%r' unexpected camera '%' after backup" % (
                server.box, camera_guid)
        time.sleep(MEDIASERVER_MERGE_TIMEOUT_SEC / 10.)


def test_backup_restore(env, camera, version):
    env.two.merge_systems(env.one)
    full_info_initial = wait_for_merge_and_get_full_info(env)
    backup = env.one.rest_api.ec2.dumpDatabase.GET()
    camera_guid = env.two.add_camera(camera)
    full_info_with_new_camera = wait_for_merge_and_get_full_info(env)
    assert full_info_with_new_camera != full_info_initial, (
        "ec2/getFullInfo data before and after saveCamera are the same")
    env.one.rest_api.ec2.restoreDatabase.POST(data=backup['data'])
    wait_camera_disappearance_after_backup(env.one, camera_guid)
    full_info_after_backup_restore = wait_for_merge_and_get_full_info(env)
    assert full_info_after_backup_restore == full_info_initial


# To detect VMS-5969
@pytest.mark.skip(reason="VMS-5969")
@pytest.mark.parametrize('version', ['current'])
def test_server_guids_changed(env):
    env.one.stop_service()
    env.two.stop_service()
    # To make server database and configuration file guids different
    env.one.reset_config(removeDbOnStartup=0, guidIsHWID='no',
                         serverGuid=BOX_1_SERVER_GUID)
    env.two.reset_config(removeDbOnStartup=0, guidIsHWID='no',
                         serverGuid=BOX_2_SERVER_GUID)
    env.one.start_service()
    env.two.start_service()
    env.one.setup_local_system()
    env.two.setup_local_system()
    env.two.merge_systems(env.one)
    wait_for_merge_and_get_full_info(env)
