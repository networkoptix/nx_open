import pytest

from framework.installation.mediaserver import Mediaserver
from framework.waiting import wait_for_equal, wait_for_truthy, ensure_persistence


@pytest.mark.parametrize('layout_file', [
    'triangle.net.yaml',
    'square.net.yaml',
    'three-proxy-three.net.yaml',
    ])
def test_update_installed(system, update_info, updates_set):
    mediaserver = next(iter(system.values()))  # type: Mediaserver  # Random mediaserver.
    mediaserver.api.start_update(update_info)
    wait_for_equal(mediaserver.api.get_update_information, update_info)
    wait_for_truthy(mediaserver.api.updates_are_ready_to_install, timeout_sec=120)
    with mediaserver.api.waiting_for_restart(timeout_sec=120):
        mediaserver.api.install_update()
    assert mediaserver.api.get_version() == updates_set.version


@pytest.mark.parametrize('layout_file', ['triangle-low_space.net.yaml'])
def test_update_low_space(system, update_info, updates_set):
    mediaserver = next(iter(system.values()))  # type: Mediaserver  # Random mediaserver.
    mediaserver.api.start_update(update_info)
    wait_for_equal(mediaserver.api.get_update_information, update_info)
    ensure_persistence(
        lambda: not mediaserver.api.updates_are_ready_to_install(),
        "updates are ready to install",
        timeout_sec=30)
