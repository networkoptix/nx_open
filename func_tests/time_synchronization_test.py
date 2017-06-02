'''Time synchronization test

   It tests the time synchronization between mediaservers
'''

import pytest
import socket
import struct
import time
import logging
import pytz
from datetime import datetime, timedelta
from test_utils.host import ProcessError
from contextlib import closing
import test_utils.utils as utils
from test_utils.utils import SimpleNamespace

log = logging.getLogger(__name__)


POSIX_RFC868_DIFF_SEC = 2208988800  # difference between POSIX and RFC868 time, seconds
RFC868_SERVER_ADDRESS = 'time.rfc868server.com'
RFC868_SERVER_PORT = 37

MAX_TIME_DIFF = timedelta(seconds=2)  # Max time difference (system<->server or server<->server), milliseconds
TIME_SERVER_WAIT_TIMEOUT_SEC = 3      # Timeout for time server response, seconds
SYNC_TIMEOUT_SEC = 2*60               # Waiting synchronization timeout, seconds
BASE_TIME = datetime(2017, 3, 14, 15, 0, 0, tzinfo=pytz.utc)  # Tue Mar 14 15:00:00 UTC 2017


def datetime_to_str(date_time):
    return date_time.strftime('%Y-%m-%d %H:%M:%S.%f %Z')


def time_tuple_to_str(date_time, duration):
    return '%s (<->%s)' % (datetime_to_str(date_time), duration)


def measure_call_duration(fn):
    def wrapper(*args, **kw):
        start_time = utils.datetime_utc_now()
        date_time = fn(*args, **kw)
        duration = utils.datetime_utc_now() - start_time
        assert isinstance(date_time, datetime)
        assert duration < MAX_TIME_DIFF
        return (date_time, duration)
    return wrapper


class TimeServer(object):

    def __init__(self, address, port):
        self.address = address
        self.port = port

    @measure_call_duration
    def get_time(self):
        '''Get time from rfc868 time server'''
        with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
            s.connect((self.address, self.port))
            time_data = s.recv(4)
            time_rfc868, = struct.unpack('!I', time_data)
            return utils.datetime_utc_from_timestamp(time_rfc868 - POSIX_RFC868_DIFF_SEC)


@pytest.fixture
def time_server():
    return TimeServer(RFC868_SERVER_ADDRESS, RFC868_SERVER_PORT)


def create_server(box, server_factory, time_server, name, box_time):
    box = box('timeless', provision_scripts=['box-provision-disable-time-protocol.sh'], sync_time=False)
    config_file_params = dict(ecInternetSyncTimePeriodSec=3, ecMaxInternetTimeSyncRetryPeriodSec=3)
    server = server_factory(name, box=box, start=False, config_file_params=config_file_params)
    set_box_time(server, box_time)
    turn_rfc868_off_and_assert_time_server_is_unreachable(box, time_server)
    server.start_service()
    server.setup_local_system()
    return (server, box)

@pytest.fixture
def env(box, server_factory, time_server):
    one, one_box = create_server(box, server_factory, time_server, 'one', BASE_TIME)
    two, two_box = create_server(box, server_factory, time_server, 'two', BASE_TIME - timedelta(hours=20))
    one.merge([two])
    env = SimpleNamespace(
        time_server=time_server,
        one=one,
        two=two,
        one_box=one_box,
        two_box=two_box,
        )
    discover_primary_server(env)
    wait_for_server_and_box_time_synced(env.primary, env.primary_box)
    wait_for_servers_time_synced(env)
    return env


def set_box_time(server, new_time):
    server.date_change_time = utils.datetime_utc_now()
    server.base_time = new_time
    server.host.run_command(['date', '-u', '--set=@%d' % (
        utils.datetime_utc_to_timestamp(new_time))])


@measure_call_duration
def get_server_current_time(server):
    time_response = server.rest_api.ec2.getCurrentTime.GET()
    server_time_data = float(time_response['value'])
    return utils.datetime_utc_from_timestamp(server_time_data / 1000.)


@measure_call_duration
def get_box_time(box):
    box_time_data = float(box.host.run_command(['date', '-u', '+%s.%N']))
    return utils.datetime_utc_from_timestamp(box_time_data)


def discover_primary_server(env):
    time_response_one = env.one.rest_api.ec2.getCurrentTime.GET()
    time_response_two = env.two.rest_api.ec2.getCurrentTime.GET()
    one_is_primary = time_response_one['isPrimaryTimeServer']
    two_is_primary = time_response_two['isPrimaryTimeServer']
    assert not (one_is_primary and two_is_primary), 'Both servers are reported to be primary'
    if one_is_primary:
        env.primary = env.one
        env.primary_box = env.one_box
        env.secondary = env.two
        env.secondary_box = env.two_box
    elif two_is_primary:
        env.primary = env.two
        env.primary_box = env.two_box
        env.secondary = env.one
        env.secondary_box = env.one_box
    else:
        assert False, 'There is no primary server in the system'


def switch_primary_and_secondary(env):
    env.primary, env.secondary = env.secondary, env.primary
    env.primary_box, env.secondary_box = env.secondary_box, env.primary_box


def wait_for_server_and_box_time_synced(server, box):
    '''Wait box(system) & mediaserver time synchronization'''
    start = time.time()
    while True:
        box_time, box_request_duration = get_box_time(box)
        server_time, server_request_duration = get_server_current_time(server)
        allowable_diff = MAX_TIME_DIFF + box_request_duration + server_request_duration
        time_diff = abs(server_time - box_time)
        log.debug("%r time: '%s', system: '%s', difference: '%s', allowable difference: '%s'",
                  box, time_tuple_to_str(server_time, server_request_duration),
                  time_tuple_to_str(box_time, box_request_duration), time_diff, allowable_diff)
        if time_diff <= allowable_diff:
            return
        if time.time() - start >= SYNC_TIMEOUT_SEC:
            assert False, (
                "%r: too much time difference (%s) between box (system): '%s' and server: '%s'" % (
                    box, time_diff, datetime_to_str(box_time), datetime_to_str(server_time)))
        time.sleep(SYNC_TIMEOUT_SEC / 10.0)


def wait_for_servers_time_synced(env):
    '''Wait 2 mediaservers time synchronization'''
    start = time.time()
    while True:
        server_time_1, server_request_duration_1 = get_server_current_time(env.one)
        server_time_2, server_request_duration_2 = get_server_current_time(env.two)
        allowable_diff = MAX_TIME_DIFF + server_request_duration_1 + server_request_duration_2
        time_diff = abs(server_time_1 - server_time_2)
        log.debug("Compare server times %r time: '%s' with %r time: '%s', difference: '%s', allowable difference: '%s'",
                  env.one_box, time_tuple_to_str(server_time_1, server_request_duration_1),
                  env.two_box, time_tuple_to_str(server_time_2, server_request_duration_2),
                  time_diff, allowable_diff)
        if time_diff <= allowable_diff:
            return
        if time.time() - start >= SYNC_TIMEOUT_SEC:
            assert False, (
                "Too much time difference (%s) between: %r: '%s' and %r: '%s'" % (
                    time_diff, env.one_box, datetime_to_str(server_time_1),
                    env.two_box, datetime_to_str(server_time_2)))
        time.sleep(SYNC_TIMEOUT_SEC / 10.0)


def assert_rfc868_server_is_unreachable(box, time_server):
    with pytest.raises(ProcessError) as x_info:
        box.host.run_command(
            ['nc', '-w', str(TIME_SERVER_WAIT_TIMEOUT_SEC),
             time_server.address, str(time_server.port)])
    assert x_info.value.returncode == 1


def assert_server_time_matches(server, expected_time, time_request_duration=timedelta(0)):
    server_time, server_request_duration = get_server_current_time(server)
    allowable_diff = MAX_TIME_DIFF + server_request_duration + time_request_duration
    time_diff = abs(server_time - expected_time)
    log.debug("Server %r time: '%s' compare with '%s' -> difference: '%s', allowable difference: '%s'",
              server, time_tuple_to_str(server_time, server_request_duration),
              time_tuple_to_str(expected_time, server_request_duration),
              time_diff, allowable_diff)
    assert time_diff <= allowable_diff, (
        "Too much time difference (%s) between: %r: '%s' and expected: '%s" % (
            time_diff, server, datetime_to_str(server_time), datetime_to_str(expected_time)))


def assert_server_and_box_time_match(env, server):
    expected_time = env.primary.base_time + (
      utils.datetime_utc_now() - env.primary.date_change_time)
    assert_server_time_matches(server, expected_time)


def get_iptables_rfc868_rule_command(command):
    return ['iptables', command, 'OUTPUT', '-p',
            'tcp', '--dport', '37', '-j', 'DROP']


def does_iptables_rfc868_rule_exist(box):
    try:
        box.host.run_command(get_iptables_rfc868_rule_command('-C'))
        return True
    except ProcessError:
        return False


def turn_rfc868_on_and_assert_time_server_is_reachable(box, time_server):
    if does_iptables_rfc868_rule_exist(box):
        box.host.run_command(get_iptables_rfc868_rule_command('-D'))
        box.host.run_command(
            ['nc', '-w', str(TIME_SERVER_WAIT_TIMEOUT_SEC),
             time_server.address, str(time_server.port)])


def turn_rfc868_off_and_assert_time_server_is_unreachable(box, time_server):
    if not does_iptables_rfc868_rule_exist(box):
        box.host.run_command(get_iptables_rfc868_rule_command('-A'))
        assert_rfc868_server_is_unreachable(box, time_server)


def wait_for_server_and_rfc868_time_synced(env, server):
    '''Wait internet time server & mediaserver time synchronization'''
    start = time.time()
    while True:
        inet_time, inet_request_duration = env.time_server.get_time()
        server_time, server_request_duration = get_server_current_time(server)
        allowable_diff = MAX_TIME_DIFF + inet_request_duration + server_request_duration
        time_diff = abs(server_time - inet_time)
        log.debug("%r time: '%s', internet time: '%s' -> difference: '%s', allowable difference: '%s'",
                  server, time_tuple_to_str(server_time, server_request_duration),
                  time_tuple_to_str(inet_time, inet_request_duration), time_diff, allowable_diff)
        if time_diff <= allowable_diff:
            return
        if time.time() - start >= SYNC_TIMEOUT_SEC:
            assert False, (
                "%r: too much time difference (%s) between internet '%s:': '%s' and server: '%s'" % (
                    server, time_diff, datetime_to_str(inet_time), datetime_to_str(server_time)))
        time.sleep(SYNC_TIMEOUT_SEC / 10.0)


def assert_server_and_rfc868_time_match(env, server):
    expected_time, time_request_duration = env.time_server.get_time()
    assert_server_time_matches(server, expected_time, time_request_duration)


def test_initial(env):
    assert_rfc868_server_is_unreachable(env.one_box, env.time_server)
    assert_rfc868_server_is_unreachable(env.two_box, env.time_server)
    assert_server_and_box_time_match(env, env.primary)


def test_change_primary_server(env):
    env.secondary.rest_api.ec2.forcePrimaryTimeServer.POST(id=env.secondary.ecs_guid)
    switch_primary_and_secondary(env)
    set_box_time(env.primary, BASE_TIME + timedelta(hours=5))
    wait_for_server_and_box_time_synced(env.primary, env.primary_box)
    wait_for_servers_time_synced(env)
    assert_server_and_box_time_match(env, env.primary)


def test_change_time_on_primary_server(env):
    set_box_time(env.primary, BASE_TIME + timedelta(hours=20))
    wait_for_server_and_box_time_synced(env.primary, env.primary_box)
    wait_for_servers_time_synced(env)
    assert_server_and_box_time_match(env, env.primary)


def test_change_time_on_secondary_server(env):
    set_box_time(env.secondary, BASE_TIME + timedelta(hours=10))
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    assert_server_and_box_time_match(env, env.secondary)


def test_primary_server_temporary_offline(env):
    assert_server_and_box_time_match(env, env.secondary)
    env.primary.stop_service()
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    assert_server_and_box_time_match(env, env.secondary)
    env.secondary.restart()
    assert_server_and_box_time_match(env, env.secondary)
    env.primary.start_service()
    discover_primary_server(env)
    set_box_time(env.primary, BASE_TIME + timedelta(hours=15))
    wait_for_server_and_box_time_synced(env.primary, env.primary_box)
    wait_for_servers_time_synced(env)
    assert_server_and_box_time_match(env, env.secondary)


# This isn't specified now. Wait for a specification.
#
# def test_secondary_server_alone(env):
#     assert_server_and_box_time_match(env, env.secondary)
#     env.primary.service.stop()
#     env.secondary.rest_api.ec2.removeResource.POST(id=env.primary.ecs_guid)
#     servers = env.secondary.rest_api.ec2.getMediaServersEx.GET()
#     assert len(servers) == 1 and servers[0]['id'] == env.secondary.ecs_guid
#     time_response_secondary = env.secondary.rest_api.ec2.getCurrentTime.GET()
#     assert time_response_secondary['isPrimaryTimeServer']
#     env.primary, env.primary_box = env.secondary, env.secondary_box
#     env.secondary = env.secondary_box = None
#     set_box_time(env.primary, BASE_TIME - timedelta(hours=15))
#     wait_for_server_and_box_time_synced(env.primary, env.primary_box)


def test_secondary_server_temporary_inet_on(env):
    # Turn on RFC868 (time protocol) on secondary box
    turn_rfc868_on_and_assert_time_server_is_reachable(env.secondary_box, env.time_server)
    wait_for_server_and_rfc868_time_synced(env, env.primary)
    wait_for_server_and_rfc868_time_synced(env, env.secondary)
    # Change system time on primary box
    set_box_time(env.primary, BASE_TIME - timedelta(hours=5))
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    assert_server_and_rfc868_time_match(env, env.primary)
    # Turn off RFC868 (time protocol)
    turn_rfc868_off_and_assert_time_server_is_unreachable(env.secondary_box, env.time_server)
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    assert_server_and_rfc868_time_match(env, env.primary)
    # Stop secondary server
    env.secondary.stop_service()
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    assert_server_and_rfc868_time_match(env, env.primary)
    env.secondary.start_service()
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    assert_server_and_rfc868_time_match(env, env.primary)
    # Restart secondary server
    env.secondary.restart()
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    assert_server_and_rfc868_time_match(env, env.primary)
    # Stop and start both servers - so that servers forget internet time
    env.secondary.stop_service()
    env.primary.stop_service()
    time.sleep(1.0)
    env.secondary.start_service()
    env.primary.start_service()
    # Detect new primary and change its system time
    discover_primary_server(env)
    set_box_time(env.primary, BASE_TIME - timedelta(hours=25))
    wait_for_server_and_box_time_synced(env.primary, env.primary_box)
    wait_for_servers_time_synced(env)
    assert_server_and_box_time_match(env, env.secondary)
