'''
Backup storage test
'''

import logging
import pytest
import time
import pytz
import os
import server_api_data_generators as generator
import test_utils.utils as utils
from datetime import datetime
from test_utils.camera import Camera
from test_utils.host import ProcessError

log = logging.getLogger(__name__)

BACKUP_STORAGE_PATH = '/tmp/bstorage'
BACKUP_STORAGE_READY_TIMEOUT_SEC = 60  # seconds
BACKUP_SCHEDULE_TIMEOUT_SEC = 60       # seconds

# Camera backup types
BACKUP_HIGH = 1  # backup only high quality stream
BACKUP_LOW = 2   # backup only low quality stream
BACKUP_BOTH = 3  # backup both (high & low) streams

# Backup and archive subpaths
HI_QUALITY_PATH = 'hi_quality'
LOW_QUALITY_PATH = 'low_quality'

# Backup settings

# Combination (via "|") of the days of week on which the backup is active on,
BACKUP_EVERY_DAY = 0x7f  # 0x7f means every day.

# Duration of the synchronization period in seconds.
BACKUP_DURATION_NOT_SET = -1  # -1 means not set.

# Maximum backup bitrate in bytes per second.
BACKUP_BITRATE_NOT_LIMITED = -1  # -1 means not limited.


@pytest.fixture(params=['low', 'high', 'both'])
def backup_type(request):
    if request.param == 'low':
        return BACKUP_LOW
    elif request.param == 'high':
        return BACKUP_HIGH
    else:
        return BACKUP_BOTH


@pytest.fixture
def env(env_builder, server, backup_type):
    one = server(start=False)
    built_env = env_builder(one=one)
    built_env.one.box.host.run_command(['rm', '-rfv', BACKUP_STORAGE_PATH])
    built_env.one.box.host.run_command(['mkdir', '-p', BACKUP_STORAGE_PATH])
    built_env.one.start_service()
    built_env.one.setup_local_system()
    built_env.one.set_system_settings(backupQualities=backup_type)
    add_backup_storage(built_env.one)
    change_and_assert_server_backup_type(built_env.one, 'BackupManual')
    built_env.camera = add_camera(built_env.one, camera_id=1, backup_type=backup_type)
    return built_env


def wait_storage_ready(server, stotage_guid):
    start = time.time()
    while True:
        status = server.rest_api.ec2.getStatusList.GET(id=stotage_guid)
        if status and status[0]['status'] == 'Online':
            return
        if time.time() - start >= BACKUP_STORAGE_READY_TIMEOUT_SEC:
            assert False, "%r: storage '%s' isn't ready in %s seconds" % (
                server.box, storage_guid, BACKUP_STORAGE_READY_TIMEOUT_SEC)
        time.sleep(BACKUP_STORAGE_READY_TIMEOUT_SEC / 10.)


def change_and_assert_server_backup_type(server, expected_backup_type):
    server.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=server.ecs_guid, backupType='BackupManual',
        backupDaysOfTheWeek=BACKUP_EVERY_DAY,
        backupStart=0,  # start backup at 00:00:00
        backupDuration=BACKUP_DURATION_NOT_SET,
        backupBitrate=BACKUP_BITRATE_NOT_LIMITED)
    attributes = server.rest_api.ec2.getMediaServerUserAttributesList.GET(
        id=server.ecs_guid)
    assert (expected_backup_type and
            attributes[0]['backupType'] == expected_backup_type)


def add_backup_storage(server):
    storages = server.rest_api.ec2.getStorages.GET()
    assert len(storages)
    backup_storage_data = generator.generate_storage_data(
        storage_id=1, isBackup=True,
        parentId=server.ecs_guid,
        storageType='local',
        usedForWriting=True,
        url=BACKUP_STORAGE_PATH)
    server.rest_api.ec2.saveStorage.POST(**backup_storage_data)
    storages = server.rest_api.ec2.getStorages.GET()
    backup_storage = [s for s in server.rest_api.ec2.getStorages.GET()
                      if s['id'] == backup_storage_data['id']]
    assert len(backup_storage) == 1
    wait_storage_ready(server, backup_storage_data['id'])


def wait_backup_finish(server, expected_backup_time):
    start = time.time()
    while True:
        backup_response = server.rest_api.api.backupControl.GET()
        backup_time_ms = int(backup_response['backupTimeMs'])
        backup_state = backup_response['state']
        backup_time = utils.datetime_utc_from_timestamp(backup_time_ms / 1000.)
        log.debug("'%r' api/backupControl.backupTimeMs: '%s', expected: '%s'",
                  server.box, utils.datetime_to_str(backup_time),
                  utils.datetime_to_str(expected_backup_time))
        if backup_state == 'BackupState_None' and backup_time_ms != 0:
            assert backup_time == expected_backup_time
            return
        if time.time() - start >= BACKUP_SCHEDULE_TIMEOUT_SEC:
            assert backup_state == 'BackupState_None' and backup_time == expected_backup_time
        time.sleep(BACKUP_SCHEDULE_TIMEOUT_SEC / 10.)


def add_camera(server, camera_id, backup_type):
    camera = Camera('Camera_%d' % camera_id, generator.generate_mac(camera_id))
    camera_guid = server.add_camera(camera)
    camera_attr = generator.generate_camera_user_attributes_data(
        dict(id=camera_guid, name=camera.name),
        backupType=backup_type)
    server.rest_api.ec2.saveCameraUserAttributes.POST(**camera_attr)
    return camera


def assert_path_does_not_exist(server, path):
    assert_message = "'%r': unexpected existen path '%s'" % (server.box, path)
    with pytest.raises(ProcessError, message=assert_message) as x_info:
        server.box.host.run_command(['[ -e %s ]' % path])
    assert x_info.value.returncode == 1, assert_message


def assert_backup_equal_to_archive(server, backup_type):
    if backup_type == BACKUP_HIGH:
        backup_path = os.path.join(BACKUP_STORAGE_PATH, HI_QUALITY_PATH)
        server_archive_path = os.path.join(server.storage.dir, HI_QUALITY_PATH)
        assert_path_does_not_exist(server, os.path.join(BACKUP_STORAGE_PATH, LOW_QUALITY_PATH))
    elif backup_type == BACKUP_LOW:
        backup_path = os.path.join(BACKUP_STORAGE_PATH, LOW_QUALITY_PATH)
        server_archive_path = os.path.join(server.storage.dir, LOW_QUALITY_PATH)
        assert_path_does_not_exist(server, os.path.join(BACKUP_STORAGE_PATH, HI_QUALITY_PATH))
    elif backup_type == BACKUP_BOTH:
        backup_path = BACKUP_STORAGE_PATH
        server_archive_path = server.storage.dir
    # Compare backup and archive storages
    server.box.host.run_command(['diff',
                                 '-x', '*.nxdb',  # don't compare storage databases
                                 '-r',            # recursively compare any subdirectories found
                                 '-s',            # report when two files are the same
                                 backup_path,  server_archive_path])


def test_backup_by_request(env, sample_media_file, backup_type):
    start_time = datetime(2017, 3, 27, tzinfo=pytz.utc)
    env.one.storage.save_media_sample(env.camera, start_time, sample_media_file)
    env.one.rebuild_archive()
    env.one.rest_api.api.backupControl.GET(action='start')
    expected_backup_time=start_time + sample_media_file.duration
    wait_backup_finish(env.one, expected_backup_time)
    assert_backup_equal_to_archive(env.one, backup_type)


def test_backup_by_schedule(env, sample_media_file, backup_type):
    start_time_1 = datetime(2017, 3, 28, 9, 52, 16, tzinfo=pytz.utc)
    start_time_2 = start_time_1 + sample_media_file.duration*2
    env.one.storage.save_media_sample(env.camera, start_time_1, sample_media_file)
    env.one.storage.save_media_sample(env.camera, start_time_2, sample_media_file)
    env.one.rebuild_archive()
    env.one.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=env.one.ecs_guid, backupType='BackupSchedule',
        backupDaysOfTheWeek=BACKUP_EVERY_DAY,
        backupStart=0,  # start backup at 00:00:00
        backupDuration=BACKUP_DURATION_NOT_SET,
        backupBitrate=BACKUP_BITRATE_NOT_LIMITED)
    expected_backup_time=start_time_2 + sample_media_file.duration
    wait_backup_finish(env.one, expected_backup_time)
    assert_backup_equal_to_archive(env.one, backup_type)
