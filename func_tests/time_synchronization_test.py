"""Tests time synchronization between mediaservers."""
from __future__ import division

import logging
import socket
import struct
import time
from collections import namedtuple
from contextlib import closing, contextmanager
from datetime import datetime, timedelta

import pytest
import pytz

from test_utils.host import ProcessError
from test_utils.utils import datetime_utc_to_timestamp, datetime_utc_now

log = logging.getLogger(__name__)

RFC868_SERVER_ADDRESS = 'time.rfc868server.com'
RFC868_SERVER_PORT = 37
RFC868_SERVER_TIMEOUT_SEC = 3  # Timeout for time server response, seconds

MAX_TIME_DIFF = timedelta(seconds=2)  # Max time difference (system<->server or server<->server), milliseconds
SYNC_TIMEOUT_SEC = 10  # Waiting synchronization timeout, seconds
BASE_TIME = datetime(2017, 3, 14, 15, 0, 0, tzinfo=pytz.utc)  # Tue Mar 14 15:00:00 UTC 2017


class RunningTime(object):
    """Accounts time passed from creation and error due to network round-trip.
    >>> ours = RunningTime(datetime(2017, 11, 5, 0, 0, 10, tzinfo=pytz.utc), timedelta(seconds=4))
    >>> ours, str(ours)  # doctest: +ELLIPSIS
    (RunningTime(...2017-11-05...+/-...), '2017-11-05 ... +/- 2...')
    >>> ours.is_close_to(datetime(2017, 11, 5, 0, 0, 5, tzinfo=pytz.utc), threshold=timedelta(seconds=2))
    False
    >>> theirs = RunningTime(datetime(2017, 11, 5, 0, 0, 5, tzinfo=pytz.utc), timedelta(seconds=4))
    >>> ours.is_close_to(theirs, threshold=timedelta(seconds=2))
    True
    """

    def __init__(self, received_time, network_round_trip):
        half_network_round_trip = timedelta(seconds=network_round_trip.total_seconds() / 2)
        self._received_at = datetime.now(pytz.utc)
        self._initial = received_time + half_network_round_trip
        self._error = half_network_round_trip  # Doesn't change significantly during runtime.

    @property
    def _current(self):
        return self._initial + (datetime.now(pytz.utc) - self._received_at)

    def is_close_to(self, other, threshold=MAX_TIME_DIFF):
        if isinstance(other, self.__class__):
            return abs(self._current - other._current) <= threshold + self._error + other._error
        else:
            return abs(self._current - other) <= threshold + self._error

    def __str__(self):
        return '{} +/- {}'.format(self._current.strftime('%Y-%m-%d %H:%M:%S.%f %Z'), self._error.total_seconds())

    def __repr__(self):
        return '{self.__class__.__name__}({self:s})'.format(self=self)


def request_time_from_rfc868_time_server(address=RFC868_SERVER_ADDRESS, port=RFC868_SERVER_PORT):
    """Get time from RFC868 time server wrap into Python's datetime.
    >>> request_time_from_rfc868_time_server()  # doctest: +ELLIPSIS
    RunningTime(...)
    """
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        started_at = datetime.now(pytz.utc)
        s.connect((address, port))
        time_data = s.recv(4)
        request_duration = datetime.now(pytz.utc) - started_at
    remote_as_rfc868_timestamp, = struct.unpack('!I', time_data)
    posix_to_rfc868_diff = datetime.fromtimestamp(0, pytz.utc) - datetime(1900, 1, 1, tzinfo=pytz.utc)
    remote_as_posix_timestamp = remote_as_rfc868_timestamp - posix_to_rfc868_diff.total_seconds()
    remote_as_datetime = datetime.fromtimestamp(remote_as_posix_timestamp, pytz.utc)
    return RunningTime(remote_as_datetime, request_duration)


INET_TIME_CACHE = None


def get_inet_time():
    global INET_TIME_CACHE
    if INET_TIME_CACHE is None:
        INET_TIME_CACHE = request_time_from_rfc868_time_server()
    return INET_TIME_CACHE


def create_server(box, server_factory, name):
    box = box('timeless', sync_time=False)
    config_file_params = dict(ecInternetSyncTimePeriodSec=3, ecMaxInternetTimeSyncRetryPeriodSec=3)
    server = server_factory(name, box=box, start=False, config_file_params=config_file_params)
    return server


def discover_primary_server(*servers):
    response = servers[0].rest_api.ec2.getCurrentTime.GET()
    primary_server_guid = response['primaryTimeServerGuid']
    for i in range(len(servers)):
        if servers[i].ecs_guid == primary_server_guid:
            return servers[i:i + 1] + servers[:i] + servers[i + 1:]


System = namedtuple('System', ['primary', 'secondary'])


@pytest.fixture
def system(box, server_factory):
    one = create_server(box, server_factory, 'one')
    two = create_server(box, server_factory, 'two')
    with iptables_rfc868(one, allowed=False), iptables_rfc868(two, allowed=False):
        one.start_service()
        one.setup_local_system()
        two.start_service()
        two.setup_local_system()
        one.merge([two])
        primary, secondary = discover_primary_server(one, two)
        system = System(primary, secondary)
        primary_box_time = set_box_time(system.primary, BASE_TIME)
        set_box_time(system.secondary, BASE_TIME)
        assert wait_until(lambda: request_server_time(system.primary).is_close_to(primary_box_time))
        assert wait_until(lambda: request_server_time(system.secondary).is_close_to(primary_box_time))
        yield system  # Mind it's within WITH statement.


@contextmanager
def swapped_primary_and_secondary(system):
    system.secondary.rest_api.ec2.forcePrimaryTimeServer.POST(id=system.secondary.ecs_guid)
    try:
        yield system.secondary, system.primary  # New primary and non-primary servers.
    finally:
        system.primary.rest_api.ec2.forcePrimaryTimeServer.POST(id=system.primary.ecs_guid)


def set_box_time(server, new_time):
    server.date_change_time = datetime_utc_now()
    server.base_time = new_time
    server.host.run_command(['date', '-u', '--set=@%d' % (datetime_utc_to_timestamp(new_time))])
    return RunningTime(new_time, datetime.now(pytz.utc) - server.date_change_time)


def request_server_time(server):
    started_at = datetime.now(pytz.utc)
    time_response = server.rest_api.ec2.getCurrentTime.GET()
    received = datetime.fromtimestamp(float(time_response['value']) / 1000., pytz.utc)
    return RunningTime(received, datetime.now(pytz.utc) - started_at)


def wait_until(check_condition, *args, **kwargs):
    start_timestamp = time.time()
    delay_sec = 0.1
    while True:
        if check_condition(*args, **kwargs):
            return True
        if time.time() - start_timestamp >= SYNC_TIMEOUT_SEC:
            return False
        time.sleep(delay_sec)
        delay_sec *= 2


def holds_long_enough(check_condition, *args, **kwargs):
    return not wait_until(lambda: not check_condition(*args, **kwargs))


@contextmanager
def iptables_rfc868(box, allowed=True, iptables_rule_target='REJECT'):
    def run_iptables_command(iptables_rule_command):
        box.host.run_command([
            'iptables', iptables_rule_command, 'OUTPUT',
            '--protocol', 'tcp', '--dport', '37',
            '--jump', iptables_rule_target])

    def connection_can_be_established(box):
        command = ['nc', '-w', str(RFC868_SERVER_TIMEOUT_SEC), RFC868_SERVER_ADDRESS, str(RFC868_SERVER_PORT)]
        try:
            box.host.run_command(command)
        except ProcessError as e:
            assert e.returncode == 1
            return False
        else:
            return True

    def append_rule():
        try:
            run_iptables_command('--check')
        except ProcessError as e:
            if e.returncode != 1:
                raise
            run_iptables_command('--append')
        assert wait_until(lambda: not connection_can_be_established(box))

    def delete_rule():
        while True:
            try:
                run_iptables_command('--delete')
            except ProcessError as e:
                if e.returncode != 1:
                    raise
                break
        assert wait_until(lambda: connection_can_be_established(box))

    if allowed:
        delete_rule()
        try:
            yield
        finally:
            append_rule()
    else:
        append_rule()
        try:
            yield
        finally:
            delete_rule()


@contextmanager
def synchronize_time_with_internet_setting(is_enabled, server):
    server.set_system_settings(synchronizeTimeWithInternet=is_enabled)
    try:
        yield
    finally:
        server.set_system_settings(synchronizeTimeWithInternet=not is_enabled)


@contextmanager
def server_stopped(server):
    server.stop_service()
    try:
        yield
    finally:
        server.start_service()


def test_primary_follows_box_time(system):
    primary_box_time = set_box_time(system.primary, BASE_TIME + timedelta(hours=20))
    assert wait_until(lambda: request_server_time(system.primary).is_close_to(primary_box_time)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
            request_server_time(system.primary), primary_box_time))


def test_change_time_on_primary_server(system):
    """Change time on PRIMARY server's machine. Expect all servers align with it."""
    primary_box_time = set_box_time(system.primary, BASE_TIME + timedelta(hours=20))
    assert wait_until(lambda: request_server_time(system.primary).is_close_to(primary_box_time)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
            request_server_time(system.primary), primary_box_time))
    assert wait_until(lambda: request_server_time(system.secondary).is_close_to(primary_box_time)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            request_server_time(system.secondary), primary_box_time))


def test_change_primary_server(system):
    """Change PRIMARY server, change time on its machine. Expect all servers align with it."""
    with swapped_primary_and_secondary(system) as (new_primary, new_secondary):
        new_primary_box_time = set_box_time(new_primary, BASE_TIME + timedelta(hours=5))
        assert wait_until(lambda: request_server_time(new_primary).is_close_to(new_primary_box_time)), (
            "Time on NEW PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH NEW PRIMARY time server %s." % (
                request_server_time(new_primary), new_primary_box_time))
        assert wait_until(lambda: request_server_time(new_secondary).is_close_to(new_primary_box_time)), (
            "Time on NEW NON-PRIMARY time server %s does NOT FOLLOW time on NEW PRIMARY time server %s." % (
                request_server_time(new_secondary), new_primary_box_time))


def test_change_time_on_secondary_server(system):
    """Change time on NON-PRIMARY server's machine. Expect all servers' time doesn't change."""
    primary_time = request_server_time(system.primary)
    set_box_time(system.secondary, BASE_TIME + timedelta(hours=10))
    assert holds_long_enough(lambda: request_server_time(system.secondary).is_close_to(primary_time)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            request_server_time(system.secondary), primary_time))


def test_primary_server_temporary_offline(system):
    primary_time = set_box_time(system.primary, BASE_TIME - timedelta(hours=2))
    assert wait_until(lambda: request_server_time(system.secondary).is_close_to(primary_time))
    with server_stopped(system.primary):
        set_box_time(system.secondary, BASE_TIME + timedelta(hours=4))
        assert holds_long_enough(lambda: request_server_time(system.secondary).is_close_to(primary_time)), (
            "After PRIMARY time server was stopped, "
            "time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
                request_server_time(system.secondary), primary_time))


def test_secondary_server_temporary_inet_on(system):
    with synchronize_time_with_internet_setting(True, system.primary):
        # Turn on RFC868 (time protocol) on secondary box
        with iptables_rfc868(system.secondary, allowed=True):
            assert wait_until(lambda: request_server_time(system.primary).is_close_to(get_inet_time())), (
                "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
                "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
                    request_server_time(system.primary), get_inet_time()))
            assert wait_until(lambda: request_server_time(system.secondary).is_close_to(get_inet_time())), (
                "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
                "its time %s is NOT EQUAL to internet time %s" % (
                    request_server_time(system.secondary), get_inet_time()))

            # Change system time on PRIMARY box
            set_box_time(system.primary, BASE_TIME - timedelta(hours=5))
            assert holds_long_enough(lambda: request_server_time(system.primary).is_close_to(get_inet_time())), (
                "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
                "time on PRIMARY time server %s does NOT FOLLOW to internet time %s" % (
                    request_server_time(system.primary), get_inet_time()))

        # Turn off RFC868 (time protocol)
        assert holds_long_enough(lambda: request_server_time(system.primary).is_close_to(get_inet_time())), (
            "After connection to internet time server "
            "on NON-PRIMARY time server was again restricted, "
            "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
                request_server_time(system.primary), get_inet_time()))

        # Stop secondary server
        with server_stopped(system.secondary):
            assert holds_long_enough(lambda: request_server_time(system.primary).is_close_to(get_inet_time())), (
                "When NON-PRIMARY server was stopped, "
                "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
                    request_server_time(system.primary), get_inet_time()))

        # Restart secondary server
        system.secondary.restart()
        assert holds_long_enough(lambda: request_server_time(system.primary).is_close_to(get_inet_time())), (
            "After NON-PRIMARY time server restart via API, "
            "its time %s is NOT EQUAL to internet time %s" % (
                request_server_time(system.primary), get_inet_time()))

        # Stop and start both servers - so that servers could forget internet time
        with server_stopped(system.secondary), server_stopped(system.primary):
            time.sleep(1)

        # Detect new PRIMARY and change its system time
        set_box_time(system.primary, BASE_TIME - timedelta(hours=25))
        assert wait_until(lambda: request_server_time(system.primary).is_close_to(get_inet_time())), (
            "After servers restart as services "
            "and changing system time on MACHINE WITH PRIMARY time server, "
            "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
                request_server_time(system.primary), get_inet_time()))
        assert wait_until(lambda: request_server_time(system.secondary).is_close_to(get_inet_time())), (
            "After servers restart as services "
            "and changing system time on MACHINE WITH PRIMARY time server, "
            "time on NON-PRIMARY time server %s is NOT EQUAL to internet time %s" % (
                request_server_time(system.secondary), get_inet_time()))
