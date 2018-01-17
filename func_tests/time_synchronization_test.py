"""Tests time synchronization between mediaservers."""
import logging
import time
from collections import namedtuple
from datetime import datetime, timedelta

import pytest
import pytz

from conftest import timeless_server
from test_utils.internet_time import get_internet_time, TimeProtocolRestriction
from test_utils.utils import wait_until, holds_long_enough

log = logging.getLogger(__name__)

BASE_TIME = datetime(2017, 3, 14, 15, 0, 0, tzinfo=pytz.utc)  # Tue Mar 14 15:00:00 UTC 2017

System = namedtuple('System', ['primary', 'secondary'])


@pytest.fixture
def system(box, server_factory):
    one = timeless_server(box, server_factory, 'one')
    two = timeless_server(box, server_factory, 'two')
    one.merge([two])
    response = one.rest_api.ec2.getCurrentTime.GET()
    primary_server_guid = response['primaryTimeServerGuid']
    system = System(one, two) if one.ecs_guid == primary_server_guid else System(two, one)
    primary_box_time = system.primary.host.set_time(BASE_TIME)
    system.secondary.host.set_time(BASE_TIME)
    assert wait_until(lambda: system.primary.get_time().is_close_to(primary_box_time)), (
        "Time %s on PRIMARY time server doesn't align with time %s on MACHINE WITH PRIMARY time server." % (
            system.primary.get_time(), primary_box_time))
    assert wait_until(lambda: system.secondary.get_time().is_close_to(primary_box_time)), (
        "Time %s on NON-PRIMARY time server doesn't align with time %s on MACHINE WITH PRIMARY time server." % (
            system.secondary.get_time(), primary_box_time))
    return system


def test_primary_follows_box_time(system):
    primary_box_time = system.primary.host.set_time(BASE_TIME + timedelta(hours=20))
    assert wait_until(lambda: system.primary.get_time().is_close_to(primary_box_time)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
            system.primary.get_time(), primary_box_time))


def test_change_time_on_primary_server(system):
    """Change time on PRIMARY server's machine. Expect all servers align with it."""
    primary_box_time = system.primary.host.set_time(BASE_TIME + timedelta(hours=20))
    assert wait_until(lambda: system.primary.get_time().is_close_to(primary_box_time)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
            system.primary.get_time(), primary_box_time))
    assert wait_until(lambda: system.secondary.get_time().is_close_to(primary_box_time)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            system.secondary.get_time(), primary_box_time))


def test_change_primary_server(system):
    """Change PRIMARY server, change time on its machine. Expect all servers align with it."""
    system.secondary.rest_api.ec2.forcePrimaryTimeServer.POST(id=system.secondary.ecs_guid)
    new_primary, new_secondary = system.secondary, system.primary
    new_primary_box_time = new_primary.host.set_time(BASE_TIME + timedelta(hours=5))
    assert wait_until(lambda: new_primary.get_time().is_close_to(new_primary_box_time)), (
        "Time on NEW PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH NEW PRIMARY time server %s." % (
            new_primary.get_time(), new_primary_box_time))
    assert wait_until(lambda: new_secondary.get_time().is_close_to(new_primary_box_time)), (
        "Time on NEW NON-PRIMARY time server %s does NOT FOLLOW time on NEW PRIMARY time server %s." % (
            new_secondary.get_time(), new_primary_box_time))


def test_change_time_on_secondary_server(system):
    """Change time on NON-PRIMARY server's machine. Expect all servers' time doesn't change."""
    primary_time = system.primary.get_time()
    system.secondary.host.set_time(BASE_TIME + timedelta(hours=10))
    assert holds_long_enough(lambda: system.secondary.get_time().is_close_to(primary_time)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            system.secondary.get_time(), primary_time))


def test_primary_server_temporary_offline(system):
    primary_time = system.primary.host.set_time(BASE_TIME - timedelta(hours=2))
    assert wait_until(lambda: system.secondary.get_time().is_close_to(primary_time))
    system.primary.stop_service()
    system.secondary.host.set_time(BASE_TIME + timedelta(hours=4))
    assert holds_long_enough(lambda: system.secondary.get_time().is_close_to(primary_time)), (
        "After PRIMARY time server was stopped, "
        "time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            system.secondary.get_time(), primary_time))


def test_secondary_server_temporary_inet_on(system):
    system.primary.set_system_settings(synchronizeTimeWithInternet=True)

    # Turn on RFC868 (time protocol) on secondary box
    TimeProtocolRestriction(system.secondary).disable()
    assert wait_until(lambda: system.primary.get_time().is_close_to(get_internet_time())), (
        "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
        "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            system.primary.get_time(), get_internet_time()))
    assert wait_until(lambda: system.secondary.get_time().is_close_to(get_internet_time())), (
        "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
        "its time %s is NOT EQUAL to internet time %s" % (
            system.secondary.get_time(), get_internet_time()))  # Change system time on PRIMARY box
    system.primary.host.set_time(BASE_TIME - timedelta(hours=5))
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
    system.primary.host.set_time(BASE_TIME - timedelta(hours=25))
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
