import logging
import time
from datetime import datetime

import pytest
import pytz
from pathlib2 import Path

import framework.utils as utils
import server_api_data_generators as generator
from framework.os_access.exceptions import NonZeroExitStatus
from framework.waiting import wait_for_truthy

_logger = logging.getLogger(__name__)


BACKUP_STORAGE_READY_TIMEOUT_SEC = 60  # seconds
BACKUP_SCHEDULE_TIMEOUT_SEC = 60       # seconds
BACKUP_STORAGE_APPEARS_TIMEOUT_SEC = 60

# Camera backup types
BACKUP_DISABLED = 0  # backup disabled
BACKUP_HIGH = 1      # backup only high quality stream
BACKUP_LOW = 2       # backup only low quality stream
BACKUP_BOTH = 3      # backup both (high & low) streams

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

# Backup volume settings

# Default size for backup storage volume
DEFAULT_VOLUME_SIZE = 10 * 1024 * 1024 * 1024


# Global system 'backupQualities' setting parametrization:
#   * low - CameraBackup_LowQuality, backup only high quality media stream;
#   * high - CameraBackup_HighQuality, backup only low quality media stream;
#   * both - CameraBackup_BothQuality, backup both (high & low quality) media streams.
@pytest.fixture(params=['low', 'high', 'both'])
def system_backup_type(request):
    if request.param == 'low':
        return BACKUP_LOW
    elif request.param == 'high':
        return BACKUP_HIGH
    else:
        return BACKUP_BOTH


# Global system 'backupNewCamerasByDefault' setting parametrization:
#  * backup - backup new cameras by default;
#  * skip - skip new cameras backup.
@pytest.fixture(params=['backup', 'skip'])
def backup_new_camera(request):
    if request.param == 'backup':
        return True
    else:
        return False


# Second camera 'backupType' attribute parametrization:
#   * default - backup attribute isn't set;
#   * disabled - CameraBackup_Disabled.
@pytest.fixture(params=['default', 'disabled'])
def second_camera_backup_type(request):
    if request.param == 'disabled':
        return BACKUP_DISABLED
    else:
        return None


@pytest.fixture()
def backup_storage_volume(one_mediaserver):
    storage_volume = one_mediaserver.os_access.make_fake_disk('backup', DEFAULT_VOLUME_SIZE)
    return storage_volume


@pytest.fixture()
def server(one_mediaserver, system_backup_type, backup_storage_volume):
    config_file_params = dict(minStorageSpace=1024*1024)  # 1M
    one_mediaserver.installation.update_mediaserver_conf(config_file_params)
    one_mediaserver.start()
    one_mediaserver.api.setup_local_system()
    one_mediaserver.api.generic.get('api/systemSettings', params=dict(backupQualities=system_backup_type))
    return one_mediaserver


def wait_storage_ready(server, storage_guid):
    start = time.time()
    while True:
        status = server.api.generic.get('ec2/getStatusList', params=dict(id=storage_guid))
        if status and status[0]['status'] == 'Online':
            return
        if time.time() - start >= BACKUP_STORAGE_READY_TIMEOUT_SEC:
            assert False, "%r: storage '%s' isn't ready in %s seconds" % (
                server, storage_guid, BACKUP_STORAGE_READY_TIMEOUT_SEC)
        time.sleep(BACKUP_STORAGE_READY_TIMEOUT_SEC / 10.)


def change_and_assert_server_backup_type(server, expected_backup_type):
    server_guid = server.api.get_server_id()
    server.api.generic.post('ec2/saveMediaServerUserAttributes', dict(
        serverId=server_guid,
        backupType='BackupManual',
        backupDaysOfTheWeek=BACKUP_EVERY_DAY,
        backupStart=0,  # start backup at 00:00:00
        backupDuration=BACKUP_DURATION_NOT_SET,
        backupBitrate=BACKUP_BITRATE_NOT_LIMITED,
        ))
    attributes = server.api.generic.get('ec2/getMediaServerUserAttributesList', params=dict(id=server_guid))
    assert (expected_backup_type and attributes[0]['backupType'] == expected_backup_type)


def get_storage(server, storage_path):
    storage_list = [s for s in server.api.generic.get('ec2/getStorages')
                    if str(storage_path) in s['url'] and s['usedForWriting']]
    if storage_list:
        assert len(storage_list) == 1
        return storage_list[0]
    else:
        return None

def add_backup_storage(server, storage_path):
    storage = wait_for_truthy(
        lambda: get_storage(server, storage_path),
        BACKUP_STORAGE_APPEARS_TIMEOUT_SEC)
    assert storage, 'Mediaserver did not accept storage %r in %r seconds' % (
        storage_path, BACKUP_STORAGE_APPEARS_TIMEOUT_SEC)
    storage['isBackup'] = True
    server.api.generic.post('ec2/saveStorage', dict(**storage))
    wait_storage_ready(server, storage['id'])
    change_and_assert_server_backup_type(server, 'BackupManual')
    return Path(storage['url'])


def wait_backup_finish(server, expected_backup_time):
    start = time.time()
    while True:
        backup_response = server.api.generic.get('api/backupControl')
        backup_time_ms = int(backup_response['backupTimeMs'])
        backup_state = backup_response['state']
        backup_time = utils.datetime_utc_from_timestamp(backup_time_ms / 1000.)
        _logger.debug(
            "'%r' api/backupControl.backupTimeMs: '%s', expected: '%s'",
            server, utils.datetime_to_str(backup_time),
            utils.datetime_to_str(expected_backup_time))
        if backup_state == 'BackupState_None' and backup_time_ms != 0:
            assert backup_time == expected_backup_time
            return
        if time.time() - start >= BACKUP_SCHEDULE_TIMEOUT_SEC:
            assert backup_state == 'BackupState_None' and backup_time == expected_backup_time
        time.sleep(BACKUP_SCHEDULE_TIMEOUT_SEC / 10.)


def add_camera(camera_pool, server, camera_id, backup_type):
    camera = camera_pool.add_camera('Camera_%d' % camera_id, generator.generate_mac(camera_id))
    camera_guid = server.api.add_camera(camera)
    if backup_type:
        camera_attr = generator.generate_camera_user_attributes_data(
            dict(id=camera_guid, name=camera.name),
            backupType=backup_type)
        server.api.generic.post('ec2/saveCameraUserAttributes', dict(**camera_attr))
    return camera


def assert_path_does_not_exist(server, path):
    assert_message = "'%r': unexpected existen path '%s'" % (server, path)
    with pytest.raises(NonZeroExitStatus, message=assert_message) as x_info:
        server.os_access.run_command(['[', '-e', path, ']'])
    assert x_info.value.exit_status == 1, assert_message


def assert_paths_are_equal(server, path_1, path_2):
    server.os_access.run_command(['diff', '--recursive', '--report-identical-files', path_1, path_2])


def assert_backup_equal_to_archive(
        server, backup_storage_path, camera,
        system_backup_type, backup_new_camera, camera_backup_type):
    hi_quality_backup_path = backup_storage_path / HI_QUALITY_PATH / camera.mac_addr
    hi_quality_server_archive_path = server.storage.dir / HI_QUALITY_PATH / camera.mac_addr
    low_quality_backup_path = backup_storage_path / LOW_QUALITY_PATH / camera.mac_addr
    low_quality_server_archive_path = server.storage.dir / LOW_QUALITY_PATH / camera.mac_addr
    if ((camera_backup_type and camera_backup_type == BACKUP_DISABLED) or
            (not camera_backup_type and not backup_new_camera)):
        assert_path_does_not_exist(server, hi_quality_backup_path)
        assert_path_does_not_exist(server, low_quality_backup_path)
    elif system_backup_type == BACKUP_HIGH:
        assert_paths_are_equal(server, hi_quality_backup_path, hi_quality_server_archive_path)
        assert_path_does_not_exist(server, low_quality_backup_path)
    elif system_backup_type == BACKUP_LOW:
        assert_paths_are_equal(server, low_quality_backup_path, low_quality_server_archive_path)
        assert_path_does_not_exist(server, hi_quality_backup_path)
    else:
        assert_paths_are_equal(server, low_quality_backup_path, low_quality_server_archive_path)
        assert_paths_are_equal(server, low_quality_backup_path, low_quality_server_archive_path)


def test_backup_by_request(
        server, backup_storage_volume, camera_pool, sample_media_file,
        system_backup_type, backup_new_camera, second_camera_backup_type):
    backup_storage_path = add_backup_storage(server, backup_storage_volume)
    server.api.generic.get('api/systemSettings', params=dict(backupNewCamerasByDefault=backup_new_camera))
    start_time = datetime(2017, 3, 27, tzinfo=pytz.utc)
    camera_1 = add_camera(camera_pool, server, camera_id=1, backup_type=BACKUP_BOTH)
    camera_2 = add_camera(camera_pool, server, camera_id=2, backup_type=second_camera_backup_type)
    server.storage.save_media_sample(camera_1, start_time, sample_media_file)
    server.storage.save_media_sample(camera_2, start_time, sample_media_file)
    server.api.rebuild_archive()
    server.api.generic.get('api/backupControl', params=dict(action='start'))
    expected_backup_time = start_time + sample_media_file.duration
    wait_backup_finish(server, expected_backup_time)
    assert_backup_equal_to_archive(
        server, backup_storage_path, camera_1, system_backup_type, backup_new_camera, camera_backup_type=BACKUP_BOTH)
    assert_backup_equal_to_archive(
        server, backup_storage_path, camera_2, system_backup_type, backup_new_camera, second_camera_backup_type)
    assert not server.installation.list_core_dumps()


def test_backup_by_schedule(server, backup_storage_volume, camera_pool, sample_media_file, system_backup_type):
    backup_storage_path = add_backup_storage(server, backup_storage_volume)
    start_time_1 = datetime(2017, 3, 28, 9, 52, 16, tzinfo=pytz.utc)
    start_time_2 = start_time_1 + sample_media_file.duration*2
    camera = add_camera(camera_pool, server, camera_id=1, backup_type=BACKUP_LOW)
    server.storage.save_media_sample(camera, start_time_1, sample_media_file)
    server.storage.save_media_sample(camera, start_time_2, sample_media_file)
    server.api.rebuild_archive()
    server.api.generic.post('ec2/saveMediaServerUserAttributes', dict(
        serverId=server.api.get_server_id(), backupType='BackupSchedule',
        backupDaysOfTheWeek=BACKUP_EVERY_DAY,
        backupStart=0,  # start backup at 00:00:00
        backupDuration=BACKUP_DURATION_NOT_SET,
        backupBitrate=BACKUP_BITRATE_NOT_LIMITED))
    expected_backup_time = start_time_2 + sample_media_file.duration
    wait_backup_finish(server, expected_backup_time)
    assert_backup_equal_to_archive(
        server, backup_storage_path, camera, system_backup_type,
        backup_new_camera=False,
        camera_backup_type=BACKUP_BOTH)
    assert not server.installation.list_core_dumps()
