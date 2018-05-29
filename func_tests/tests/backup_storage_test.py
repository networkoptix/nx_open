import logging
import re
import time
from datetime import datetime

import pytest
import pytz
from pathlib2 import Path

import framework.utils as utils
import server_api_data_generators as generator
from framework.api_shortcuts import get_server_id
from framework.merging import setup_local_system
from framework.os_access.exceptions import NonZeroExitStatus
from framework.os_access.posix_shell_utils import sh_quote_arg
from framework.waiting import wait_for_true

log = logging.getLogger(__name__)


BACKUP_STORAGE_PATH = Path('/mnt/backup')
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
DEFAULT_VOLUME_SIZE_GB = 10
ONE_GB = 1024 * 1024 * 1024
DF_SIZE_REGEX = re.compile(r'^.*\s+([0-9\.]+)G$', re.MULTILINE)


class LinuxVMVolume(object):
    def __init__(self, ssh_access):
        self._ssh_access = ssh_access  # SSH

    def _need_create(self, mount_point, size_gb):
        try:
            df_output = self._ssh_access.run_command(
                ['df', str(mount_point), '-h', '--output=size', '-BG'])
            m = DF_SIZE_REGEX.search(df_output)
            if m:
                return m.group(1) == size_gb
            return True
        except NonZeroExitStatus:
            return True

    def ensure_volume_exists(self, mount_point, size_gb=DEFAULT_VOLUME_SIZE_GB):
        if self._need_create(mount_point, size_gb):
            image_path = self._ssh_access.Path('/').joinpath(mount_point.name).with_suffix('.image')
            if image_path.exists():
                image_path.unlink()
            self._ssh_access.run_command(['fallocate', '-l', size_gb * ONE_GB, image_path])
            self._ssh_access.run_command(['/sbin/mke2fs', '-F', '-t', 'ext4', image_path])
            self._ssh_access.Path(mount_point).mkdir()
            self._ssh_access.run_command(['mount', image_path, mount_point])


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


@pytest.fixture
def linux_vm_volume(one_mediaserver):
    return LinuxVMVolume(one_mediaserver.os_access)


@pytest.fixture
def server(one_mediaserver, linux_vm_volume, system_backup_type):
    config_file_params = dict(minStorageSpace=1024*1024)  # 1M
    one_mediaserver.installation.update_mediaserver_conf(config_file_params)
    linux_vm_volume.ensure_volume_exists(BACKUP_STORAGE_PATH)
    one_mediaserver.os_access.ssh.run_sh_script(
        'rm -rfv "$STORAGE_PATH/*"',
        env=dict(STORAGE_PATH=BACKUP_STORAGE_PATH),
        )
    one_mediaserver.start()
    setup_local_system(one_mediaserver, {})
    one_mediaserver.api.api.systemSettings.GET(backupQualities=system_backup_type)
    return one_mediaserver


def wait_storage_ready(server, storage_guid):
    start = time.time()
    while True:
        status = server.api.ec2.getStatusList.GET(id=storage_guid)
        if status and status[0]['status'] == 'Online':
            return
        if time.time() - start >= BACKUP_STORAGE_READY_TIMEOUT_SEC:
            assert False, "%r: storage '%s' isn't ready in %s seconds" % (
                server, storage_guid, BACKUP_STORAGE_READY_TIMEOUT_SEC)
        time.sleep(BACKUP_STORAGE_READY_TIMEOUT_SEC / 10.)


def change_and_assert_server_backup_type(server, expected_backup_type):
    server_guid = get_server_id(server.api)
    server.api.ec2.saveMediaServerUserAttributes.POST(
        serverId=server_guid,
        backupType='BackupManual',
        backupDaysOfTheWeek=BACKUP_EVERY_DAY,
        backupStart=0,  # start backup at 00:00:00
        backupDuration=BACKUP_DURATION_NOT_SET,
        backupBitrate=BACKUP_BITRATE_NOT_LIMITED,
        )
    attributes = server.api.ec2.getMediaServerUserAttributesList.GET(id=server_guid)
    assert (expected_backup_type and attributes[0]['backupType'] == expected_backup_type)


def get_storage(server, storage_path):
    storage_list = [s for s in server.api.ec2.getStorages.GET()
                    if str(storage_path) in s['url'] and s['usedForWriting']]
    if storage_list:
        assert len(storage_list) == 1
        return storage_list[0]
    else:
        return None

def add_backup_storage(server):
    storage = wait_for_true(
        lambda: get_storage(server, BACKUP_STORAGE_PATH),
        BACKUP_STORAGE_APPEARS_TIMEOUT_SEC)
    assert storage, 'Mediaserver did not accept storage %r in %r seconds' % (
        BACKUP_STORAGE_PATH, BACKUP_STORAGE_APPEARS_TIMEOUT_SEC)
    storage['isBackup'] = True
    server.api.ec2.saveStorage.POST(**storage)
    wait_storage_ready(server, storage['id'])
    change_and_assert_server_backup_type(server, 'BackupManual')
    return Path(storage['url'])


def wait_backup_finish(server, expected_backup_time):
    start = time.time()
    while True:
        backup_response = server.api.api.backupControl.GET()
        backup_time_ms = int(backup_response['backupTimeMs'])
        backup_state = backup_response['state']
        backup_time = utils.datetime_utc_from_timestamp(backup_time_ms / 1000.)
        log.debug("'%r' api/backupControl.backupTimeMs: '%s', expected: '%s'",
                  server, utils.datetime_to_str(backup_time),
                  utils.datetime_to_str(expected_backup_time))
        if backup_state == 'BackupState_None' and backup_time_ms != 0:
            assert backup_time == expected_backup_time
            return
        if time.time() - start >= BACKUP_SCHEDULE_TIMEOUT_SEC:
            assert backup_state == 'BackupState_None' and backup_time == expected_backup_time
        time.sleep(BACKUP_SCHEDULE_TIMEOUT_SEC / 10.)


def add_camera(camera_factory, server, camera_id, backup_type):
    camera = camera_factory('Camera_%d' % camera_id, generator.generate_mac(camera_id))
    camera_guid = server.add_camera(camera)
    if backup_type:
        camera_attr = generator.generate_camera_user_attributes_data(
            dict(id=camera_guid, name=camera.name),
            backupType=backup_type)
        server.api.ec2.saveCameraUserAttributes.POST(**camera_attr)
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
        server, camera_factory, sample_media_file,
        system_backup_type, backup_new_camera, second_camera_backup_type):
    backup_storage_path = add_backup_storage(server)
    server.api.api.systemSettings.GET(
        backupNewCamerasByDefault=backup_new_camera)
    start_time = datetime(2017, 3, 27, tzinfo=pytz.utc)
    camera_1 = add_camera(camera_factory, server, camera_id=1, backup_type=BACKUP_BOTH)
    camera_2 = add_camera(camera_factory, server, camera_id=2, backup_type=second_camera_backup_type)
    server.storage.save_media_sample(camera_1, start_time, sample_media_file)
    server.storage.save_media_sample(camera_2, start_time, sample_media_file)
    server.rebuild_archive()
    server.api.api.backupControl.GET(action='start')
    expected_backup_time = start_time + sample_media_file.duration
    wait_backup_finish(server, expected_backup_time)
    assert_backup_equal_to_archive(
        server, backup_storage_path, camera_1, system_backup_type, backup_new_camera, camera_backup_type=BACKUP_BOTH)
    assert_backup_equal_to_archive(
        server, backup_storage_path, camera_2, system_backup_type, backup_new_camera, second_camera_backup_type)
    assert not server.installation.list_core_dumps()


def test_backup_by_schedule(server, camera_factory, sample_media_file, system_backup_type):
    backup_storage_path = add_backup_storage(server)
    start_time_1 = datetime(2017, 3, 28, 9, 52, 16, tzinfo=pytz.utc)
    start_time_2 = start_time_1 + sample_media_file.duration*2
    camera = add_camera(camera_factory, server, camera_id=1, backup_type=BACKUP_LOW)
    server.storage.save_media_sample(camera, start_time_1, sample_media_file)
    server.storage.save_media_sample(camera, start_time_2, sample_media_file)
    server.rebuild_archive()
    server.api.ec2.saveMediaServerUserAttributes.POST(
        serverId=get_server_id(server.api), backupType='BackupSchedule',
        backupDaysOfTheWeek=BACKUP_EVERY_DAY,
        backupStart=0,  # start backup at 00:00:00
        backupDuration=BACKUP_DURATION_NOT_SET,
        backupBitrate=BACKUP_BITRATE_NOT_LIMITED)
    expected_backup_time = start_time_2 + sample_media_file.duration
    wait_backup_finish(server, expected_backup_time)
    assert_backup_equal_to_archive(
        server, backup_storage_path, camera, system_backup_type,
        backup_new_camera=False,
        camera_backup_type=BACKUP_BOTH)
    assert not server.installation.list_core_dumps()
