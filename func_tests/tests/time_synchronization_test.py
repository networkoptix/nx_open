import logging
import time
from datetime import datetime, timedelta

import pytest
from pytz import utc

from framework.timeless_mediaserver import timeless_mediaserver
from framework.api_shortcuts import get_server_id, get_time, is_primary_time_server
from framework.merging import merge_systems
from framework.utils import RunningTime, get_internet_time
from framework.waiting import ensure_persistence, wait_for_true

_logger = logging.getLogger(__name__)

BASE_TIME = RunningTime(datetime(2017, 3, 14, 15, 0, 0, tzinfo=utc))  # Tue Mar 14 15:00:00 UTC 2017


@pytest.fixture()
def two_mediaservers(two_vms, mediaserver_installers, ca, artifact_factory):
    """Make sure mediaservers are installed, stopped and internet is disabled."""
    first_vm, second_vm = two_vms
    with timeless_mediaserver(first_vm, mediaserver_installers, ca, artifact_factory) as first:
        with timeless_mediaserver(second_vm, mediaserver_installers, ca, artifact_factory) as second:
            merge_systems(first, second)
            primary, secondary = (first, second) if is_primary_time_server(first.api) else (second, first)
            secondary.os_access.set_time(BASE_TIME.current)
            primary.os_access.set_time(BASE_TIME.current)
            return primary, secondary


@pytest.mark.quick
def test_secondary_respects_primary(two_mediaservers):
    primary, secondary = two_mediaservers
    secondary_response = secondary.api.get('ec2/getCurrentTime')
    assert not secondary_response['isPrimaryTimeServer']
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


@pytest.mark.quick
def test_primary_follows_vm_time(two_mediaservers):
    primary, secondary = two_mediaservers
    wait_for_true(
        lambda: get_time(primary.api).is_close_to(BASE_TIME),
        "time on PRIMARY time server {} follows its machine time {}.".format(
            get_time(primary.api), BASE_TIME))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


@pytest.mark.quick
@pytest.mark.parametrize(
    'shift_hours',
    [-10, -1, 1, 10],
    ids=lambda hr: '{}{}hr'.format('plus' if hr > 0 else 'minus', abs(hr)))
def test_secondary_follows_primary(two_mediaservers, shift_hours):
    primary, secondary = two_mediaservers
    secondary.os_access.set_time(BASE_TIME.current + timedelta(hours=shift_hours))
    wait_for_true(
        lambda: get_time(secondary.api).is_close_to(BASE_TIME),
        "time {} on NON-PRIMARY time server aligns with time {} on MACHINE WITH PRIMARY time server.".format(
            get_time(secondary.api), BASE_TIME))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


@pytest.mark.quick
def test_change_primary_server(two_mediaservers):
    """Change PRIMARY server, change time on its machine. Expect all servers align with it."""
    old_primary, old_secondary = two_mediaservers
    old_secondary_uuid = get_server_id(old_secondary.api)
    old_secondary.api.post('ec2/forcePrimaryTimeServer', dict(id=old_secondary_uuid))
    wait_for_true(
        lambda: is_primary_time_server(old_secondary.api),
        '{} becomes primary'.format(old_primary))
    new_primary, new_secondary = old_secondary, old_primary
    new_primary_vm_time = new_primary.os_access.set_time(BASE_TIME.current + timedelta(hours=5))
    wait_for_true(
        lambda: get_time(new_primary.api).is_close_to(new_primary_vm_time),
        "time on NEW PRIMARY time server {} follows time on MACHINE WITH NEW PRIMARY time server {}".format(
            get_time(new_primary.api), new_primary_vm_time))
    wait_for_true(
        lambda: get_time(new_secondary.api).is_close_to(new_primary_vm_time),
        "time on NEW NON-PRIMARY time server {} follows time on NEW PRIMARY time server {}".format(
            get_time(new_secondary.api), new_primary_vm_time))
    assert not old_primary.installation.list_core_dumps()
    assert not old_secondary.installation.list_core_dumps()


def test_change_time_on_secondary_server(two_mediaservers):
    """Change time on NON-PRIMARY server's machine. Expect all servers' time doesn't change."""
    primary, secondary = two_mediaservers
    secondary.os_access.set_time(BASE_TIME.current + timedelta(hours=10))
    ensure_persistence(
        lambda: get_time(secondary.api).is_close_to(BASE_TIME),
        "time on NON-PRIMARY time server {} follows time on PRIMARY time server {}".format(
            get_time(secondary.api), BASE_TIME))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


def test_primary_server_temporary_offline(two_mediaservers):
    primary, secondary = two_mediaservers
    primary.stop()
    secondary.os_access.set_time(BASE_TIME.current + timedelta(hours=4))
    ensure_persistence(
        lambda: get_time(secondary.api).is_close_to(BASE_TIME),
        "time on NON-PRIMARY time server {} follows time on PRIMARY time server {}"
        "after PRIMARY time server was stopped".format(get_time(secondary.api), BASE_TIME))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


def test_secondary_server_temporary_inet_on(two_mediaservers):
    primary, secondary = two_mediaservers
    primary.api.get('api/systemSettings', params=dict(synchronizeTimeWithInternet=True))
    secondary.os_access.networking.enable_internet()

    wait_for_true(
        lambda: get_time(secondary.api).is_close_to(get_internet_time()),
        "NON-PRIMARY aligns with INTERNET while internet is enabled")
    wait_for_true(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        "PRIMARY aligns with INTERNET while internet is enabled")
    primary.os_access.set_time(BASE_TIME.current - timedelta(hours=5))
    ensure_persistence(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        "PRIMARY aligns with INTERNET after system time is shifted but while internet is enabled")
    secondary.os_access.networking.disable_internet()

    # Turn off RFC868 (time protocol)
    ensure_persistence(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        "PRIMARY aligns with INTERNET after internet was disabled")

    # Stop secondary server
    secondary.stop()
    ensure_persistence(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        "PRIMARY aligns with INTERNET while NON-PRIMARY is stopped")
    secondary.start()

    # Restart secondary server
    secondary.restart_via_api()
    ensure_persistence(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        "NON-PRIMARY aligns with INTERNET after restart via API")

    # Stop and start both servers - so that servers could forget internet time
    secondary.stop()
    primary.stop()
    time.sleep(1)
    primary.start()
    secondary.start()

    # Detect new PRIMARY and change its system time
    primary.os_access.set_time(BASE_TIME.current - timedelta(hours=25))
    wait_for_true(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        "PRIMARY aligns with INTERNET after both are restarted")
    wait_for_true(
        lambda: get_time(secondary.api).is_close_to(get_internet_time()),
        "NON-PRIMARY aligns with INTERNET after both are restarted")

    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()
