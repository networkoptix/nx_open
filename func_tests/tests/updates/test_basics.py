from typing import Any

from framework.installation.installer import InstallerSet
from framework.installation.mediaserver import Mediaserver
from framework.waiting import wait_for_equal


def test_update_info_upload(one_mediaserver_api, update_info):
    one_mediaserver_api.start_update(update_info)
    wait_for_equal(one_mediaserver_api.get_update_information, update_info)


def test_update_info_understood(one_mediaserver_api, update_info):
    one_mediaserver_api.start_update(update_info)
    wait_for_equal(one_mediaserver_api.get_update_information, update_info)
    wait_for_equal(one_mediaserver_api.get_update_status, 'downloading', path=(0, 'code'))


def test_update_file_downloaded(one_mediaserver_api, update_info):
    one_mediaserver_api.start_update(update_info)
    wait_for_equal(one_mediaserver_api.get_update_information, update_info)
    wait_for_equal(one_mediaserver_api.get_update_status, 'downloading', path=(0, 'code'))
    wait_for_equal(one_mediaserver_api.get_update_status, 'readyToInstall', path=(0, 'code'))


def test_update_installed(one_running_mediaserver, update_info, updates_set):
    # type: (Mediaserver, Any, InstallerSet) -> None
    one_mediaserver_api = one_running_mediaserver.api
    one_mediaserver_api.start_update(update_info)
    wait_for_equal(one_mediaserver_api.get_update_information, update_info)
    wait_for_equal(one_mediaserver_api.get_update_status, 'downloading', path=(0, 'code'))
    wait_for_equal(one_mediaserver_api.get_update_status, 'readyToInstall', path=(0, 'code'))
    with one_mediaserver_api.waiting_for_restart(timeout_sec=60):
        one_mediaserver_api.install_update()
    assert one_mediaserver_api.get_version() == updates_set.version
