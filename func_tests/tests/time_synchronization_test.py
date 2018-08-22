import logging
import time
from datetime import datetime, timedelta

import pytest
from pytz import utc

from framework.merging import merge_systems
from framework.timeless_mediaserver import timeless_mediaserver
from framework.utils import RunningTime, get_internet_time
from framework.waiting import ensure_persistence, wait_for_true

_logger = logging.getLogger(__name__)

BASE_TIME = RunningTime(datetime(2017, 3, 14, 15, 0, 0, tzinfo=utc))  # Tue Mar 14 15:00:00 UTC 2017
SYNC_TIMEOUT_SEC = 180


@pytest.fixture()
def two_mediaservers(mediaserver_allocation, two_vms):
    """Make sure mediaservers are installed, stopped and internet is disabled."""
    first_vm, second_vm = two_vms
    first_vm.os_access.set_time(BASE_TIME.current)
    second_vm.os_access.set_time(BASE_TIME.current)
    with timeless_mediaserver(mediaserver_allocation, first_vm) as primary:
        with timeless_mediaserver(mediaserver_allocation, second_vm) as secondary:
            merge_systems(primary, secondary)
            primary_guid = primary.api.get_server_id()
            primary.api.generic.post('ec2/forcePrimaryTimeServer', dict(id=primary_guid))
            wait_for_true(
                lambda: secondary.api.get_time().is_close_to(primary.os_access.get_time()),
                "time on NEW PRIMARY time server {} follows time on MACHINE WITH NEW PRIMARY time server {}".format(
                    secondary.api.get_time(), primary.os_access.get_time()))
            return primary, secondary


@pytest.mark.quick
def test_secondary_respects_primary(two_mediaservers):
    primary, secondary = two_mediaservers
    response = secondary.api.generic.get('api/systemSettings')
    assert response['settings']['primaryTimeServer'] == primary.api.get_server_id()
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


@pytest.mark.quick
def test_primary_follows_vm_time(two_mediaservers):
    primary, secondary = two_mediaservers
    assert primary.os_access.get_time().is_close_to(BASE_TIME)
    wait_for_true(
        lambda: primary.api.get_time().is_close_to(primary.os_access.get_time()),
        "time on PRIMARY time server {} follows its machine time {}.".format(
            primary.api.get_time(), primary.os_access.get_time()))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


@pytest.mark.quick
@pytest.mark.parametrize(
    'shift_hours',
    [-10, -1, 1, 10],
    ids=lambda hr: '{}{}hr'.format('plus' if hr > 0 else 'minus', abs(hr)))
def test_secondary_follows_primary(two_mediaservers, shift_hours):
    primary, secondary = two_mediaservers
    assert primary.os_access.get_time().is_close_to(BASE_TIME)
    secondary.os_access.set_time(BASE_TIME.current + timedelta(hours=shift_hours))
    wait_for_true(
        lambda: secondary.api.get_time().is_close_to(primary.os_access.get_time()),
        "time {} on NON-PRIMARY time server aligns with time {} on MACHINE WITH PRIMARY time server.".format(
            secondary.api.get_time(), primary.os_access.get_time()))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


@pytest.mark.quick
def test_change_primary_server(two_mediaservers):
    """Change PRIMARY server, change time on its machine. Expect all servers align with it."""
    old_primary, old_secondary = two_mediaservers
    old_secondary_uuid = old_secondary.api.get_server_id()
    old_secondary.api.generic.post('ec2/forcePrimaryTimeServer', dict(id=old_secondary_uuid))
    wait_for_true(old_secondary.api.is_primary_time_server)
    new_primary, new_secondary = old_secondary, old_primary
    new_time = BASE_TIME.current + timedelta(hours=5)
    new_primary.os_access.set_time(new_time)
    assert new_primary.os_access.get_time().is_close_to(new_time)
    wait_for_true(
        lambda: new_primary.api.get_time().is_close_to(new_primary.os_access.get_time()),
        "time on NEW PRIMARY time server {} follows time on MACHINE WITH NEW PRIMARY time server {}".format(
            new_primary.api.get_time(), new_primary.os_access.get_time()))
    wait_for_true(
        lambda: new_secondary.api.get_time().is_close_to(new_primary.os_access.get_time()),
        "time on NEW NON-PRIMARY time server {} follows time on NEW PRIMARY time server {}".format(
            new_secondary.api.get_time(), new_primary.os_access.get_time()),
        timeout_sec=SYNC_TIMEOUT_SEC)
    assert not old_primary.installation.list_core_dumps()
    assert not old_secondary.installation.list_core_dumps()


def test_change_time_on_secondary_server(two_mediaservers):
    """Change time on NON-PRIMARY server's machine. Expect all servers' time doesn't change."""
    primary, secondary = two_mediaservers
    new_secondary_vm_time = BASE_TIME.current + timedelta(hours=10)
    secondary.os_access.set_time(new_secondary_vm_time)
    assert primary.api.is_primary_time_server()
    assert not secondary.api.is_primary_time_server()
    assert primary.os_access.get_time().is_close_to(BASE_TIME)
    assert secondary.os_access.get_time().is_close_to(new_secondary_vm_time)
    ensure_persistence(
        lambda: secondary.api.get_time().is_close_to(primary.os_access.get_time()),
        "time on NON-PRIMARY time server {} follows time on PRIMARY time server {}".format(
            secondary.api.get_time(), primary.os_access.get_time()))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


def test_primary_server_temporary_offline(two_mediaservers):
    primary, secondary = two_mediaservers
    primary.stop()
    secondary.os_access.set_time(BASE_TIME.current + timedelta(hours=4))
    ensure_persistence(
        lambda: secondary.api.get_time().is_close_to(primary.os_access.get_time()),
        "time on NON-PRIMARY time server {} follows time on PRIMARY time server {}"
        "after PRIMARY time server was stopped".format(
            secondary.api.get_time(), primary.os_access.get_time()))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


def test_secondary_server_temporary_inet_on(two_mediaservers):
    primary, secondary = two_mediaservers
    primary.api.generic.post('ec2/forcePrimaryTimeServer', dict())
    secondary.os_access.networking.enable_internet()
    
    wait_for_true(
        lambda: secondary.api.get_time().is_close_to(get_internet_time()),
        "time on NON-PRIMARY {} aligns with INTERNET {} while internet is enabled".format(
            secondary.api.get_time(), (get_internet_time())),
        timeout_sec=SYNC_TIMEOUT_SEC)
    wait_for_true(
        lambda: primary.api.get_time().is_close_to(get_internet_time()),
        "time on PRIMARY {} aligns with INTERNET {} while internet is enabled".format(
            primary.api.get_time(), (get_internet_time())),
        timeout_sec=SYNC_TIMEOUT_SEC)
    primary.os_access.set_time(BASE_TIME.current - timedelta(hours=5))
    ensure_persistence(
        lambda: primary.api.get_time().is_close_to(get_internet_time()),
        "PRIMARY {} aligns with INTERNET {} after system time is shifted but while internet is enabled".format(
            primary.api.get_time(), (get_internet_time())))
    secondary.os_access.networking.disable_internet()

    # Turn off RFC868 (time protocol)
    ensure_persistence(
        lambda: primary.api.get_time().is_close_to(get_internet_time()),
        "PRIMARY {} aligns with INTERNET {} after internet was disabled".format(
            primary.api.get_time(), (get_internet_time())))

    # Stop secondary server
    secondary.stop()
    ensure_persistence(
        lambda: primary.api.get_time().is_close_to(get_internet_time()),
        "PRIMARY {} aligns with INTERNET {} while NON-PRIMARY is stopped".format(
            primary.api.get_time(), (get_internet_time())))
    secondary.start()

    # Restart secondary server
    secondary.api.restart_via_api()
    ensure_persistence(
        lambda: primary.api.get_time().is_close_to(get_internet_time()),
        "NON-PRIMARY {} aligns with INTERNET {} after restart via API".format(
            primary.api.get_time(), (get_internet_time())))

    # Stop and start both servers - so that servers could forget internet time
    secondary.stop()
    primary.stop()
    time.sleep(1)
    primary.start()
    secondary.start()

    # Change time on both servers in the system
    primary.os_access.set_time(BASE_TIME.current - timedelta(hours=25))
    secondary.os_access.set_time(BASE_TIME.current - timedelta(hours=50))
    ensure_persistence(
        lambda: primary.api.get_time().is_close_to(get_internet_time()),
        "PRIMARY {} aligns with INTERNET {} after both are restarted".format(
            primary.api.get_time(), (get_internet_time())))
    ensure_persistence(
        lambda: secondary.api.get_time().is_close_to(get_internet_time()),
        "NON-PRIMARY {} aligns with INTERNET {} after both are restarted".format(
            primary.api.get_time(), (get_internet_time())))

    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()
