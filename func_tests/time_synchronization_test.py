import logging
import time
from collections import namedtuple
from datetime import datetime, timedelta

import pytest
import pytz

from test_utils.internet_time import TimeProtocolRestriction, get_internet_time
from test_utils.networking import setup_networks
from test_utils.server import Server
from test_utils.server_installation import install_mediaserver
from test_utils.service import UpstartService
from test_utils.utils import holds_long_enough, wait_until

log = logging.getLogger(__name__)

BASE_TIME = datetime(2017, 3, 14, 15, 0, 0, tzinfo=pytz.utc)  # Tue Mar 14 15:00:00 UTC 2017

System = namedtuple('System', ['primary', 'secondary'])


def _make_timeless_server(vm, mediaserver_deb, ca, server_name):
    server_installation = install_mediaserver(vm.guest_os_access, mediaserver_deb)
    server_installation.change_config(ecInternetSyncTimePeriodSec=3, ecMaxInternetTimeSyncRetryPeriodSec=3)
    service = UpstartService(vm.guest_os_access, mediaserver_deb.customization.service_name)
    api_url = 'http://{host}:{port}/'.format(host=vm.host_os_access.hostname, port=vm.config.rest_api_forwarded_port)
    server = Server(server_name, vm.guest_os_access, service, server_installation, api_url, ca, vm)
    if service.get_state():
        server.stop_service()
    vm.networking.os_networking.prohibit_global()
    server_installation.cleanup_var_dir()
    server.start_service()
    server.setup_local_system()
    return server


@pytest.fixture()
def layout_file():
    return 'direct.yaml'


@pytest.fixture
def system(vm_factory, mediaserver_deb, ca):
    one = _make_timeless_server(vm_factory.get('first'), mediaserver_deb, ca, 'one')
    two = _make_timeless_server(vm_factory.get('second'), mediaserver_deb, ca, 'two')
    one.merge([two])
    response = one.rest_api.ec2.getCurrentTime.GET()
    primary_server_guid = response['primaryTimeServerGuid']
    system = System(one, two) if one.ecs_guid == primary_server_guid else System(two, one)
    primary_vm_time = system.primary.os_access.set_time(BASE_TIME)
    system.secondary.os_access.set_time(BASE_TIME)
    assert wait_until(lambda: system.primary.get_time().is_close_to(primary_vm_time)), (
        "Time %s on PRIMARY time server doesn't align with time %s on MACHINE WITH PRIMARY time server." % (
            system.primary.get_time(), primary_vm_time))
    assert wait_until(lambda: system.secondary.get_time().is_close_to(primary_vm_time)), (
        "Time %s on NON-PRIMARY time server doesn't align with time %s on MACHINE WITH PRIMARY time server." % (
            system.secondary.get_time(), primary_vm_time))
    return system


@pytest.mark.quick
def test_primary_follows_vm_time(system):
    primary_vm_time = system.primary.os_access.set_time(BASE_TIME + timedelta(hours=20))
    assert wait_until(lambda: system.primary.get_time().is_close_to(primary_vm_time)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
            system.primary.get_time(), primary_vm_time))


@pytest.mark.quick
def test_change_time_on_primary_server(system):
    """Change time on PRIMARY server's machine. Expect all servers align with it."""
    primary_vm_time = system.primary.os_access.set_time(BASE_TIME + timedelta(hours=20))
    assert wait_until(lambda: system.primary.get_time().is_close_to(primary_vm_time)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
            system.primary.get_time(), primary_vm_time))
    assert wait_until(lambda: system.secondary.get_time().is_close_to(primary_vm_time)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            system.secondary.get_time(), primary_vm_time))


@pytest.mark.quick
def test_change_primary_server(system):
    """Change PRIMARY server, change time on its machine. Expect all servers align with it."""
    system.secondary.rest_api.ec2.forcePrimaryTimeServer.POST(id=system.secondary.ecs_guid)
    new_primary, new_secondary = system.secondary, system.primary
    new_primary_vm_time = new_primary.os_access.set_time(BASE_TIME + timedelta(hours=5))
    assert wait_until(lambda: new_primary.get_time().is_close_to(new_primary_vm_time)), (
        "Time on NEW PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH NEW PRIMARY time server %s." % (
            new_primary.get_time(), new_primary_vm_time))
    assert wait_until(lambda: new_secondary.get_time().is_close_to(new_primary_vm_time)), (
        "Time on NEW NON-PRIMARY time server %s does NOT FOLLOW time on NEW PRIMARY time server %s." % (
            new_secondary.get_time(), new_primary_vm_time))


def test_change_time_on_secondary_server(system):
    """Change time on NON-PRIMARY server's machine. Expect all servers' time doesn't change."""
    primary_time = system.primary.get_time()
    system.secondary.os_access.set_time(BASE_TIME + timedelta(hours=10))
    assert holds_long_enough(lambda: system.secondary.get_time().is_close_to(primary_time)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            system.secondary.get_time(), primary_time))


def test_primary_server_temporary_offline(system):
    primary_time = system.primary.os_access.set_time(BASE_TIME - timedelta(hours=2))
    assert wait_until(lambda: system.secondary.get_time().is_close_to(primary_time))
    system.primary.stop_service()
    system.secondary.os_access.set_time(BASE_TIME + timedelta(hours=4))
    assert holds_long_enough(lambda: system.secondary.get_time().is_close_to(primary_time)), (
        "After PRIMARY time server was stopped, "
        "time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            system.secondary.get_time(), primary_time))


def test_secondary_server_temporary_inet_on(system):
    system.primary.set_system_settings(synchronizeTimeWithInternet=True)

    # Turn on RFC868 (time protocol) on secondary VM.
    TimeProtocolRestriction(system.secondary).disable()
    assert wait_until(lambda: system.primary.get_time().is_close_to(get_internet_time())), (
        "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
        "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            system.primary.get_time(), get_internet_time()))
    assert wait_until(lambda: system.secondary.get_time().is_close_to(get_internet_time())), (
        "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
        "its time %s is NOT EQUAL to internet time %s" % (
            system.secondary.get_time(), get_internet_time()))  # Change system time on PRIMARY VM.
    system.primary.os_access.set_time(BASE_TIME - timedelta(hours=5))
    assert holds_long_enough(lambda: system.primary.get_time().is_close_to(get_internet_time())), (
        "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
        "time on PRIMARY time server %s does NOT FOLLOW to internet time %s" % (
            system.primary.get_time(), get_internet_time()))
    TimeProtocolRestriction(system.secondary).enable()

    # Turn off RFC868 (time protocol)
    assert holds_long_enough(lambda: system.primary.get_time().is_close_to(get_internet_time())), (
        "After connection to internet time server "
        "on NON-PRIMARY time server was again restricted, "
        "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            system.primary.get_time(), get_internet_time()))

    # Stop secondary server
    system.secondary.stop_service()
    assert holds_long_enough(lambda: system.primary.get_time().is_close_to(get_internet_time())), (
        "When NON-PRIMARY server was stopped, "
        "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            system.primary.get_time(), get_internet_time()))
    system.secondary.start_service()

    # Restart secondary server
    system.secondary.restart_via_api()
    assert holds_long_enough(lambda: system.primary.get_time().is_close_to(get_internet_time())), (
        "After NON-PRIMARY time server restart via API, "
        "its time %s is NOT EQUAL to internet time %s" % (
            system.primary.get_time(), get_internet_time()))

    # Stop and start both servers - so that servers could forget internet time
    system.secondary.stop_service()
    system.primary.stop_service()
    time.sleep(1)
    system.primary.start_service()
    system.secondary.start_service()

    # Detect new PRIMARY and change its system time
    system.primary.os_access.set_time(BASE_TIME - timedelta(hours=25))
    assert wait_until(lambda: system.primary.get_time().is_close_to(get_internet_time())), (
        "After servers restart as services "
        "and changing system time on MACHINE WITH PRIMARY time server, "
        "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            system.primary.get_time(), get_internet_time()))
    assert wait_until(lambda: system.secondary.get_time().is_close_to(get_internet_time())), (
        "After servers restart as services "
        "and changing system time on MACHINE WITH PRIMARY time server, "
        "time on NON-PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            system.secondary.get_time(), get_internet_time()))
