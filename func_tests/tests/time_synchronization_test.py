import contextlib
import logging
import socket
import struct
import time
from collections import namedtuple
from datetime import datetime, timedelta

import pytest
from pylru import lrudecorator
from pytz import utc

from framework.api_shortcuts import get_server_id, get_time
from framework.merging import setup_system, setup_local_system
from framework.utils import RunningTime, holds_long_enough, wait_until

log = logging.getLogger(__name__)

BASE_TIME = datetime(2017, 3, 14, 15, 0, 0, tzinfo=utc)  # Tue Mar 14 15:00:00 UTC 2017

System = namedtuple('System', ['primary', 'secondary'])


@lrudecorator(1)
def get_internet_time(address='time.rfc868server.com', port=37):
    """Get time from RFC868 time server wrap into Python's datetime.
    >>> import timeit
    >>> get_internet_time()  # doctest: +ELLIPSIS
    RunningTime(...)
    >>> timeit.timeit(get_internet_time, number=1) < 1e-4
    True
    """
    with contextlib.closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        started_at = datetime.now(utc)
        s.connect((address, port))
        time_data = s.recv(4)
        request_duration = datetime.now(utc) - started_at
    remote_as_rfc868_timestamp, = struct.unpack('!I', time_data)
    posix_to_rfc868_diff = datetime.fromtimestamp(0, utc) - datetime(1900, 1, 1, tzinfo=utc)
    remote_as_posix_timestamp = remote_as_rfc868_timestamp - posix_to_rfc868_diff.total_seconds()
    remote_as_datetime = datetime.fromtimestamp(remote_as_posix_timestamp, utc)
    return RunningTime(remote_as_datetime, request_duration)


@pytest.fixture()
def system(two_vms, linux_servers_pool):
    servers = {}
    for vm in two_vms:
        server = linux_servers_pool.get(vm.alias, vm=vm)
        if server.service.is_running():
            server.stop()
        # Reset server without internet access.
        server.machine.networking.disable_internet()
        server.installation.cleanup_var_dir()
        server.installation.update_mediaserver_conf({
            'ecInternetSyncTimePeriodSec': 3,
            'ecMaxInternetTimeSyncRetryPeriodSec': 3,
            })
        server.start()
        setup_local_system(server, {})
        servers[vm.alias] = server
    setup_system(servers, {'first': {'second': None}})
    first_server_response = servers['first'].api.ec2.getCurrentTime.GET()
    if first_server_response['isPrimaryTimeServer']:
        system = System(primary=servers['first'], secondary=servers['second'])
    else:
        system = System(primary=servers['second'], secondary=servers['first'])
    primary_vm_time = system.primary.os_access.set_time(BASE_TIME)
    system.secondary.os_access.set_time(BASE_TIME)
    assert wait_until(lambda: get_time(system.primary.api).is_close_to(primary_vm_time)), (
        "Time %s on PRIMARY time server doesn't align with time %s on MACHINE WITH PRIMARY time server." % (
            get_time(system.primary.api), primary_vm_time))
    assert wait_until(lambda: get_time(system.secondary.api).is_close_to(primary_vm_time)), (
        "Time %s on NON-PRIMARY time server doesn't align with time %s on MACHINE WITH PRIMARY time server." % (
            get_time(system.secondary.api), primary_vm_time))
    return system


@pytest.mark.quick
def test_primary_follows_vm_time(system):
    primary_vm_time = system.primary.os_access.set_time(BASE_TIME + timedelta(hours=20))
    assert wait_until(lambda: get_time(system.primary.api).is_close_to(primary_vm_time)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
            get_time(system.primary.api), primary_vm_time))
    assert not system.primary.installation.list_core_dumps()
    assert not system.secondary.installation.list_core_dumps()


@pytest.mark.quick
def test_change_time_on_primary_server(system):
    """Change time on PRIMARY server's machine. Expect all servers align with it."""
    primary_vm_time = system.primary.os_access.set_time(BASE_TIME + timedelta(hours=20))
    assert wait_until(lambda: get_time(system.primary.api).is_close_to(primary_vm_time)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
            get_time(system.primary.api), primary_vm_time))
    assert wait_until(lambda: get_time(system.secondary.api).is_close_to(primary_vm_time)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            get_time(system.secondary.api), primary_vm_time))
    assert not system.primary.installation.list_core_dumps()
    assert not system.secondary.installation.list_core_dumps()


@pytest.mark.quick
def test_change_primary_server(system):
    """Change PRIMARY server, change time on its machine. Expect all servers align with it."""
    guid = get_server_id(system.secondary.api)
    system.secondary.api.ec2.forcePrimaryTimeServer.POST(id=guid)
    new_primary, new_secondary = system.secondary, system.primary
    new_primary_vm_time = new_primary.os_access.set_time(BASE_TIME + timedelta(hours=5))
    assert wait_until(lambda: get_time(new_primary.api).is_close_to(new_primary_vm_time)), (
        "Time on NEW PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH NEW PRIMARY time server %s." % (
            get_time(new_primary.api), new_primary_vm_time))
    assert wait_until(lambda: get_time(new_secondary.api).is_close_to(new_primary_vm_time)), (
        "Time on NEW NON-PRIMARY time server %s does NOT FOLLOW time on NEW PRIMARY time server %s." % (
            get_time(new_secondary.api), new_primary_vm_time))
    assert not system.primary.installation.list_core_dumps()
    assert not system.secondary.installation.list_core_dumps()


def test_change_time_on_secondary_server(system):
    """Change time on NON-PRIMARY server's machine. Expect all servers' time doesn't change."""
    primary_time = get_time(system.primary.api)
    system.secondary.os_access.set_time(BASE_TIME + timedelta(hours=10))
    assert holds_long_enough(lambda: get_time(system.secondary.api).is_close_to(primary_time)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            get_time(system.secondary.api), primary_time))
    assert not system.primary.installation.list_core_dumps()
    assert not system.secondary.installation.list_core_dumps()


def test_primary_server_temporary_offline(system):
    primary_time = system.primary.os_access.set_time(BASE_TIME - timedelta(hours=2))
    assert wait_until(lambda: get_time(system.secondary.api).is_close_to(primary_time))
    system.primary.stop()
    system.secondary.os_access.set_time(BASE_TIME + timedelta(hours=4))
    assert holds_long_enough(lambda: get_time(system.secondary.api).is_close_to(primary_time)), (
        "After PRIMARY time server was stopped, "
        "time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            get_time(system.secondary.api), primary_time))
    assert not system.primary.installation.list_core_dumps()
    assert not system.secondary.installation.list_core_dumps()


def test_secondary_server_temporary_inet_on(system):
    system.primary.api.api.systemSettings.GET(synchronizeTimeWithInternet=True)

    system.secondary.machine.networking.enable_internet()
    assert wait_until(
        lambda: get_time(system.secondary.api).is_close_to(get_internet_time()),
        name="until NON-PRIMARY aligns with INTERNET while internet is enabled")
    assert wait_until(
        lambda: get_time(system.primary.api).is_close_to(get_internet_time()),
        name="until PRIMARY aligns with INTERNET while internet is enabled")
    system.primary.os_access.set_time(BASE_TIME - timedelta(hours=5))
    assert holds_long_enough(
        lambda: get_time(system.primary.api).is_close_to(get_internet_time()),
        name="until PRIMARY aligns with INTERNET after system time is shifted but while internet is enabled")
    system.secondary.machine.networking.disable_internet()

    # Turn off RFC868 (time protocol)
    assert holds_long_enough(
        lambda: get_time(system.primary.api).is_close_to(get_internet_time()),
        name="until PRIMARY aligns with INTERNET after internet was disabled")

    # Stop secondary server
    system.secondary.stop()
    assert holds_long_enough(
        lambda: get_time(system.primary.api).is_close_to(get_internet_time()),
        name="until PRIMARY aligns with INTERNET while NON-PRIMARY is stopped")
    system.secondary.start()

    # Restart secondary server
    system.secondary.restart_via_api()
    assert holds_long_enough(
        lambda: get_time(system.primary.api).is_close_to(get_internet_time()),
        name="until NON-PRIMARY aligns with INTERNET after restart via API")

    # Stop and start both servers - so that servers could forget internet time
    system.secondary.stop()
    system.primary.stop()
    time.sleep(1)
    system.primary.start()
    system.secondary.start()

    # Detect new PRIMARY and change its system time
    system.primary.os_access.set_time(BASE_TIME - timedelta(hours=25))
    assert wait_until(
        lambda: get_time(system.primary.api).is_close_to(get_internet_time()),
        name="until PRIMARY aligns with INTERNET after both are restarted")
    assert wait_until(
        lambda: get_time(system.secondary.api).is_close_to(get_internet_time()),
        name="until NON-PRIMARY aligns with INTERNET after both are restarted")
    
    assert not system.primary.installation.list_core_dumps()
    assert not system.secondary.installation.list_core_dumps()
