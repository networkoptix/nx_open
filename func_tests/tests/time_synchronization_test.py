import logging
import time
from contextlib import contextmanager
from datetime import datetime, timedelta

import pytest
from pytz import utc

from framework.api_shortcuts import get_server_id, get_time, is_primary_time_server
from framework.mediaserver_factory import (
    cleanup_mediaserver,
    collect_artifacts_from_mediaserver,
    make_dirty_mediaserver,
    )
from framework.merging import merge_systems, setup_local_system
from framework.utils import get_internet_time, holds_long_enough, wait_until, RunningTime

log = logging.getLogger(__name__)

BASE_TIME = RunningTime(datetime(2017, 3, 14, 15, 0, 0, tzinfo=utc))  # Tue Mar 14 15:00:00 UTC 2017


@contextmanager
def timeless_mediaserver(vm, mediaserver_deb, ca, artifact_factory):
    """Mediaserver never exposed to internet depending on machine time"""
    mediaserver = make_dirty_mediaserver(vm.alias, vm, mediaserver_deb)
    mediaserver.stop(already_stopped_ok=True)
    vm.networking.disable_internet()
    cleanup_mediaserver(mediaserver, ca)
    mediaserver.installation.update_mediaserver_conf({
        'ecInternetSyncTimePeriodSec': 3,
        'ecMaxInternetTimeSyncRetryPeriodSec': 3,
        })
    try:
        yield mediaserver
    finally:
        collect_artifacts_from_mediaserver(mediaserver, artifact_factory)


@pytest.fixture()
def two_mediaservers(two_linux_vms, mediaserver_deb, ca, artifact_factory):
    """Make sure mediaservers are installed, stopped and internet is disabled."""
    first_vm, second_vm = two_linux_vms
    with timeless_mediaserver(first_vm, mediaserver_deb, ca, artifact_factory) as first:
        with timeless_mediaserver(second_vm, mediaserver_deb, ca, artifact_factory) as second:
            for mediaserver in (first, second):
                mediaserver.start()
                setup_local_system(mediaserver, {})
            merge_systems(first, second)
            primary, secondary = (first, second) if is_primary_time_server(first.api) else (second, first)
            secondary.machine.os_access.set_time(BASE_TIME.current)
            primary.machine.os_access.set_time(BASE_TIME.current)
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
    assert wait_until(lambda: get_time(primary.api).is_close_to(BASE_TIME)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
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
    secondary.machine.os_access.set_time(BASE_TIME.current + timedelta(hours=shift_hours))
    assert wait_until(lambda: get_time(secondary.api).is_close_to(BASE_TIME)), (
        "Time %s on NON-PRIMARY time server doesn't align with time %s on MACHINE WITH PRIMARY time server." % (
            get_time(secondary.api), BASE_TIME))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


@pytest.mark.quick
def test_change_primary_server(two_mediaservers):
    """Change PRIMARY server, change time on its machine. Expect all servers align with it."""
    old_primary, old_secondary = two_mediaservers
    old_secondary_uuid = get_server_id(old_secondary.api)
    old_secondary.api.ec2.forcePrimaryTimeServer.POST(id=old_secondary_uuid)
    assert wait_until(
        lambda: is_primary_time_server(old_secondary.api),
        name='until {!r} becomes primary'.format(old_primary))
    new_primary, new_secondary = old_secondary, old_primary
    new_primary_vm_time = new_primary.machine.os_access.set_time(BASE_TIME.current + timedelta(hours=5))
    assert wait_until(lambda: get_time(new_primary.api).is_close_to(new_primary_vm_time)), (
        "Time on NEW PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH NEW PRIMARY time server %s." % (
            get_time(new_primary.api), new_primary_vm_time))
    assert wait_until(lambda: get_time(new_secondary.api).is_close_to(new_primary_vm_time)), (
        "Time on NEW NON-PRIMARY time server %s does NOT FOLLOW time on NEW PRIMARY time server %s." % (
            get_time(new_secondary.api), new_primary_vm_time))
    assert not old_primary.installation.list_core_dumps()
    assert not old_secondary.installation.list_core_dumps()


def test_change_time_on_secondary_server(two_mediaservers):
    """Change time on NON-PRIMARY server's machine. Expect all servers' time doesn't change."""
    primary, secondary = two_mediaservers
    secondary.machine.os_access.set_time(BASE_TIME.current + timedelta(hours=10))
    assert holds_long_enough(lambda: get_time(secondary.api).is_close_to(BASE_TIME)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            get_time(secondary.api), BASE_TIME))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


def test_primary_server_temporary_offline(two_mediaservers):
    primary, secondary = two_mediaservers
    primary.stop()
    secondary.machine.os_access.set_time(BASE_TIME.current + timedelta(hours=4))
    assert holds_long_enough(lambda: get_time(secondary.api).is_close_to(BASE_TIME)), (
        "After PRIMARY time server was stopped, "
        "time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            get_time(secondary.api), BASE_TIME))
    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()


def test_secondary_server_temporary_inet_on(two_mediaservers):
    primary, secondary = two_mediaservers
    primary.api.api.systemSettings.GET(synchronizeTimeWithInternet=True)
    secondary.machine.networking.enable_internet()

    assert wait_until(
        lambda: get_time(secondary.api).is_close_to(get_internet_time()),
        name="until NON-PRIMARY aligns with INTERNET while internet is enabled")
    assert wait_until(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        name="until PRIMARY aligns with INTERNET while internet is enabled")
    primary.machine.os_access.set_time(BASE_TIME.current - timedelta(hours=5))
    assert holds_long_enough(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        name="until PRIMARY aligns with INTERNET after system time is shifted but while internet is enabled")
    secondary.machine.networking.disable_internet()

    # Turn off RFC868 (time protocol)
    assert holds_long_enough(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        name="until PRIMARY aligns with INTERNET after internet was disabled")

    # Stop secondary server
    secondary.stop()
    assert holds_long_enough(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        name="until PRIMARY aligns with INTERNET while NON-PRIMARY is stopped")
    secondary.start()

    # Restart secondary server
    secondary.restart_via_api()
    assert holds_long_enough(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        name="until NON-PRIMARY aligns with INTERNET after restart via API")

    # Stop and start both servers - so that servers could forget internet time
    secondary.stop()
    primary.stop()
    time.sleep(1)
    primary.start()
    secondary.start()

    # Detect new PRIMARY and change its system time
    primary.machine.os_access.set_time(BASE_TIME.current - timedelta(hours=25))
    assert wait_until(
        lambda: get_time(primary.api).is_close_to(get_internet_time()),
        name="until PRIMARY aligns with INTERNET after both are restarted")
    assert wait_until(
        lambda: get_time(secondary.api).is_close_to(get_internet_time()),
        name="until NON-PRIMARY aligns with INTERNET after both are restarted")

    assert not primary.installation.list_core_dumps()
    assert not secondary.installation.list_core_dumps()
