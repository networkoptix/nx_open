"""Tests time synchronization between mediaservers."""
import logging
import socket
import struct
import time
from collections import namedtuple
from contextlib import closing
from datetime import datetime, timedelta

import pytest
import pytz

from test_utils.host import ProcessError

log = logging.getLogger(__name__)

RFC868_SERVER_ADDRESS = 'time.rfc868server.com'
RFC868_SERVER_PORT = 37
RFC868_SERVER_TIMEOUT_SEC = 3  # Timeout for time server response, seconds

MAX_TIME_DIFF = timedelta(seconds=2)  # Max time difference (system<->server or server<->server), milliseconds
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


def wait_until(check_condition, timeout_sec=10):
    start_timestamp = time.time()
    delay_sec = 0.1
    while True:
        if check_condition():
            return True
        if time.time() - start_timestamp >= timeout_sec:
            return False
        time.sleep(delay_sec)
        delay_sec *= 2


def holds_long_enough(check_condition, timeout_sec=10):
    return not wait_until(lambda: not check_condition(), timeout_sec=timeout_sec)


def _request_internet_time_server(address=RFC868_SERVER_ADDRESS, port=RFC868_SERVER_PORT):
    """Get time from RFC868 time server wrap into Python's datetime.
    >>> _request_internet_time_server()  # doctest: +ELLIPSIS
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


_INET_TIME_CACHE = None


def _get_inet_time():
    global _INET_TIME_CACHE
    if _INET_TIME_CACHE is None:
        _INET_TIME_CACHE = _request_internet_time_server()
    return _INET_TIME_CACHE


class TimeProtocolRestriction(object):
    def __init__(self, server):
        self.server = server

    def _run_iptables_command(self, iptables_rule_command):
        self.server.host.run_command([
            'iptables', iptables_rule_command, 'OUTPUT',
            '--protocol', 'tcp', '--dport', '37',
            '--jump', 'REJECT'])

    def _can_connect_to_time_server(self):
        command = ['nc', '-w', str(RFC868_SERVER_TIMEOUT_SEC), RFC868_SERVER_ADDRESS, str(RFC868_SERVER_PORT)]
        try:
            self.server.host.run_command(command)
        except ProcessError as e:
            assert e.returncode == 1, "Unexpected return code when checking connection to internet time server."
            return False
        else:
            return True

    def enable(self):
        try:
            self._run_iptables_command('--check')
        except ProcessError as e:
            assert e.returncode == 1, "Unexpected return code when checking if iptables rule exists."
            self._run_iptables_command('--append')
        assert wait_until(lambda: not self._can_connect_to_time_server()), (
            "Connected to internet time server while iptables rule exists.")

    def disable(self):
        while True:
            try:
                self._run_iptables_command('--delete')
            except ProcessError as e:
                assert e.returncode == 1, "Unexpected return code when deleting non-existent iptables rule."
                break
        assert wait_until(lambda: self._can_connect_to_time_server()), (
            "Cannot connect to internet time server while iptables rule doesn't exist.")


def _create_timeless_server(box, server_factory, name):
    box = box('timeless', sync_time=False)
    config_file_params = dict(ecInternetSyncTimePeriodSec=3, ecMaxInternetTimeSyncRetryPeriodSec=3)
    server = server_factory(name, box=box, start=False, config_file_params=config_file_params)
    TimeProtocolRestriction(server).enable()
    server.start_service()
    server.setup_local_system()
    return server


def _discover_primary_server(one, two):
    """Rearrange couple of servers so that primary goes first."""
    response = one.rest_api.ec2.getCurrentTime.GET()
    primary_server_guid = response['primaryTimeServerGuid']
    return (one, two) if one.ecs_guid == primary_server_guid else (two, one)


System = namedtuple('System', ['primary', 'secondary'])


def _set_box_time(server, new_time):
    started_at = datetime.now(pytz.utc)
    server.host.run_command(['date', '--set', new_time.isoformat()])
    return RunningTime(new_time, datetime.now(pytz.utc) - started_at)


def _request_server_time(server):
    started_at = datetime.now(pytz.utc)
    time_response = server.rest_api.ec2.getCurrentTime.GET()
    received = datetime.fromtimestamp(float(time_response['value']) / 1000., pytz.utc)
    return RunningTime(received, datetime.now(pytz.utc) - started_at)


@pytest.fixture
def system(box, server_factory):
    one = _create_timeless_server(box, server_factory, 'one')
    two = _create_timeless_server(box, server_factory, 'two')
    one.merge([two])
    primary, secondary = _discover_primary_server(one, two)
    system = System(primary, secondary)
    primary_box_time = _set_box_time(system.primary, BASE_TIME)
    _set_box_time(system.secondary, BASE_TIME)
    assert wait_until(lambda: _request_server_time(system.primary).is_close_to(primary_box_time)), (
        "Time %s on PRIMARY time server doesn't align with time %s on MACHINE WITH PRIMARY time server." % (
            _request_server_time(system.primary), primary_box_time))
    assert wait_until(lambda: _request_server_time(system.secondary).is_close_to(primary_box_time)), (
        "Time %s on NON-PRIMARY time server doesn't align with time %s on MACHINE WITH PRIMARY time server." % (
            _request_server_time(system.secondary), primary_box_time))
    return system


def test_primary_follows_box_time(system):
    primary_box_time = _set_box_time(system.primary, BASE_TIME + timedelta(hours=20))
    assert wait_until(lambda: _request_server_time(system.primary).is_close_to(primary_box_time)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
            _request_server_time(system.primary), primary_box_time))


def test_change_time_on_primary_server(system):
    """Change time on PRIMARY server's machine. Expect all servers align with it."""
    primary_box_time = _set_box_time(system.primary, BASE_TIME + timedelta(hours=20))
    assert wait_until(lambda: _request_server_time(system.primary).is_close_to(primary_box_time)), (
        "Time on PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH PRIMARY time server %s." % (
            _request_server_time(system.primary), primary_box_time))
    assert wait_until(lambda: _request_server_time(system.secondary).is_close_to(primary_box_time)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            _request_server_time(system.secondary), primary_box_time))


def test_change_primary_server(system):
    """Change PRIMARY server, change time on its machine. Expect all servers align with it."""
    system.secondary.rest_api.ec2.forcePrimaryTimeServer.POST(id=system.secondary.ecs_guid)
    new_primary, new_secondary = system.secondary, system.primary
    new_primary_box_time = _set_box_time(new_primary, BASE_TIME + timedelta(hours=5))
    assert wait_until(lambda: _request_server_time(new_primary).is_close_to(new_primary_box_time)), (
        "Time on NEW PRIMARY time server %s does NOT FOLLOW time on MACHINE WITH NEW PRIMARY time server %s." % (
            _request_server_time(new_primary), new_primary_box_time))
    assert wait_until(lambda: _request_server_time(new_secondary).is_close_to(new_primary_box_time)), (
        "Time on NEW NON-PRIMARY time server %s does NOT FOLLOW time on NEW PRIMARY time server %s." % (
            _request_server_time(new_secondary), new_primary_box_time))


def test_change_time_on_secondary_server(system):
    """Change time on NON-PRIMARY server's machine. Expect all servers' time doesn't change."""
    primary_time = _request_server_time(system.primary)
    _set_box_time(system.secondary, BASE_TIME + timedelta(hours=10))
    assert holds_long_enough(lambda: _request_server_time(system.secondary).is_close_to(primary_time)), (
        "Time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            _request_server_time(system.secondary), primary_time))


def test_primary_server_temporary_offline(system):
    primary_time = _set_box_time(system.primary, BASE_TIME - timedelta(hours=2))
    assert wait_until(lambda: _request_server_time(system.secondary).is_close_to(primary_time))
    system.primary.stop_service()
    _set_box_time(system.secondary, BASE_TIME + timedelta(hours=4))
    assert holds_long_enough(lambda: _request_server_time(system.secondary).is_close_to(primary_time)), (
        "After PRIMARY time server was stopped, "
        "time on NON-PRIMARY time server %s does NOT FOLLOW time on PRIMARY time server %s." % (
            _request_server_time(system.secondary), primary_time))


def test_secondary_server_temporary_inet_on(system):
    system.primary.set_system_settings(synchronizeTimeWithInternet=True)

    # Turn on RFC868 (time protocol) on secondary box
    TimeProtocolRestriction(system.secondary).disable()
    assert wait_until(lambda: _request_server_time(system.primary).is_close_to(_get_inet_time())), (
        "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
        "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            _request_server_time(system.primary), _get_inet_time()))
    assert wait_until(lambda: _request_server_time(system.secondary).is_close_to(_get_inet_time())), (
        "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
        "its time %s is NOT EQUAL to internet time %s" % (
            _request_server_time(system.secondary), _get_inet_time()))  # Change system time on PRIMARY box
    _set_box_time(system.primary, BASE_TIME - timedelta(hours=5))
    assert holds_long_enough(lambda: _request_server_time(system.primary).is_close_to(_get_inet_time())), (
        "After connection to internet time server on MACHINE WITH NON-PRIMARY time server was allowed, "
        "time on PRIMARY time server %s does NOT FOLLOW to internet time %s" % (
            _request_server_time(system.primary), _get_inet_time()))
    TimeProtocolRestriction(system.secondary).enable()

    # Turn off RFC868 (time protocol)
    assert holds_long_enough(lambda: _request_server_time(system.primary).is_close_to(_get_inet_time())), (
        "After connection to internet time server "
        "on NON-PRIMARY time server was again restricted, "
        "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            _request_server_time(system.primary), _get_inet_time()))

    # Stop secondary server
    system.secondary.stop_service()
    assert holds_long_enough(lambda: _request_server_time(system.primary).is_close_to(_get_inet_time())), (
        "When NON-PRIMARY server was stopped, "
        "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            _request_server_time(system.primary), _get_inet_time()))
    system.secondary.start_service()

    # Restart secondary server
    system.secondary.restart()
    assert holds_long_enough(lambda: _request_server_time(system.primary).is_close_to(_get_inet_time())), (
        "After NON-PRIMARY time server restart via API, "
        "its time %s is NOT EQUAL to internet time %s" % (
            _request_server_time(system.primary), _get_inet_time()))

    # Stop and start both servers - so that servers could forget internet time
    system.secondary.stop_service()
    system.primary.stop_service()
    time.sleep(1)
    system.primary.start_service()
    system.secondary.start_service()

    # Detect new PRIMARY and change its system time
    _set_box_time(system.primary, BASE_TIME - timedelta(hours=25))
    assert wait_until(lambda: _request_server_time(system.primary).is_close_to(_get_inet_time())), (
        "After servers restart as services "
        "and changing system time on MACHINE WITH PRIMARY time server, "
        "time on PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            _request_server_time(system.primary), _get_inet_time()))
    assert wait_until(lambda: _request_server_time(system.secondary).is_close_to(_get_inet_time())), (
        "After servers restart as services "
        "and changing system time on MACHINE WITH PRIMARY time server, "
        "time on NON-PRIMARY time server %s is NOT EQUAL to internet time %s" % (
            _request_server_time(system.secondary), _get_inet_time()))
