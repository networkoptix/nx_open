'''Time synchronization test

   It tests the time synchronization between mediaservers
'''

import pytest
import socket
import struct
import time
import logging
from test_utils.host import ProcessError

log = logging.getLogger(__name__)

POSIX_RFC868_DIFF_SEC = 2208988800 # difference between POSIX and RFC868 time, seconds
RFC868_SERVER_ADDRESS = 'instance1.rfc868server.com'
RFC868_SERVER_PORT = 37

MAX_TIME_DIFF = 2000               # Max time difference (system<->server or server<->server), milliseconds
TIME_SERVER_WAIT_TIMEOUT_SEC = 3   # Timeout for time server response, seconds
SYNC_TIMEOUT_SEC = 60              # Waiting synchronization timeout, seconds
BASE_TIME = 1489502441             # Tue Mar 14 14:40:41 UTC 2017

def seconds_to_str(milliseconds):
  if milliseconds == 0: return '0'
  sec = int(milliseconds / 1000)
  mcs = int(milliseconds % 1000)
  return '%s.%03d' % (time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(sec)), mcs)

class TimeServer(object):

    def __init__(self, address, port):
        self.address = address
        self.port = port

    def get_time(self):
        '''Get time from rfc868 time server'''
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((self.address, self.port))
        time_data = s.recv(4)
        time_rfc868, = struct.unpack('!I', time_data) 
        return (time_rfc868 - POSIX_RFC868_DIFF_SEC)*1000

@pytest.fixture
def time_server():
    return TimeServer(RFC868_SERVER_ADDRESS, RFC868_SERVER_PORT)

@pytest.fixture
def env(env_builder, box, server, time_server):
    box1 = box('timeless', provision_scripts=['box-provision-disable-time-protocol.sh'], sync_time=False)
    box2 = box('timeless', provision_scripts=['box-provision-disable-time-protocol.sh'], sync_time=False)
    config_file_params = {'ecInternetSyncTimePeriodSec': 3,
                          'ecMaxInternetTimeSyncRetryPeriodSec' : 3 }
    one = server(box=box1, start=False, config_file_params=config_file_params)
    two = server(box=box2, start=False, config_file_params=config_file_params)
    build_env = env_builder(one=one, two=two)
    build_env.time_server = time_server
    set_time_on_server(build_env.one, BASE_TIME)
    set_time_on_server(build_env.two, BASE_TIME - 20*60*60)
    turn_inet_off_and_check_time_server(build_env.one.box, build_env.time_server)
    turn_inet_off_and_check_time_server(build_env.two.box, build_env.time_server)
    build_env.one.start_service()
    build_env.two.start_service()
    build_env.one.setup_local_system()
    build_env.two.setup_local_system()
    build_env.one.merge_systems(build_env.two)
    build_env.primary, build_env.secondary = get_primary_server(build_env)
    wait_server_box_sync(build_env.primary, build_env.primary.box)
    wait_servers_sync(build_env)
    return build_env

def set_time_on_server(server, new_time = 0):
    server.date_change_time = int(time.time())
    server.base_time = new_time*1000
    server.box.host.run_command(['date',  '--set=@%d' % new_time])

def get_server_current_time(server):
    time_response = server.rest_api.ec2.getCurrentTime.GET()
    server.is_primary = time_response['isPrimaryTimeServer'] # Mark server primary
    server_time_data = time_response['value']
    return float(server_time_data)

def get_primary_server(env):
    get_server_current_time(env.one)
    get_server_current_time(env.two)
    return (env.one, env.two) if env.one.is_primary else (env.two, env.one)

def wait_server_box_sync(server, box):
    '''Wait box(system) & mediaserver time synchronization'''
    start = time.time()
    while True:
        box_time_data = box.host.run_command(['date', '+%s.%N'])
        box_time = float(box_time_data) * 1000.0
        server_time = get_server_current_time(server)
        log.debug("%r time: '%s', system: '%s'", server.box, 
                  seconds_to_str(server_time), seconds_to_str(box_time))
        time_diff = abs(server_time - box_time)
        if time_diff <= MAX_TIME_DIFF:
            return
        if time.time() - start >= SYNC_TIMEOUT_SEC:
            assert False, (
                "%r: too much time difference (%s) between box (system): '%s' and server: '%s'" % (
                server.box, time_diff, seconds_to_str(box_time), seconds_to_str(server_time)))
        time.sleep(SYNC_TIMEOUT_SEC / 10.0)

def wait_servers_sync(env):
    '''Wait 2 mediaservers time synchronization'''
    start = time.time()
    while True:
        server_time_1 = get_server_current_time(env.one)
        server_time_2 = get_server_current_time(env.two)
        log.debug("Compare server times %r time: '%s' with %r time: '%s'",
            env.one.box, seconds_to_str(server_time_1),
            env.two.box, seconds_to_str(server_time_2))
        time_diff = abs(server_time_1 - server_time_2)
        if time_diff <= MAX_TIME_DIFF:
            return
        if time.time() - start >= SYNC_TIMEOUT_SEC:
            assert False, (
                "Too much time difference (%s) between: %r: '%s' and %r: '%s'" % (
                time_diff, env.one.box, seconds_to_str(server_time_1),
                env.two.box, seconds_to_str(server_time_2)))
        time.sleep(SYNC_TIMEOUT_SEC / 10.0)

def check_time_server_is_unreachable(box, time_server):
    with pytest.raises(ProcessError) as x_info:
        box.host.run_command(
            ['nc', '-w', str(TIME_SERVER_WAIT_TIMEOUT_SEC),
             time_server.address, str(time_server.port)])
    assert x_info.value.returncode == 1

def check_server_time(env, server, get_exp_time_fn):
    expected_time = get_exp_time_fn(env)
    server_time = get_server_current_time(server)
    log.debug("Server %r time: '%s' compare with '%s'", server.box,
              seconds_to_str(server_time), seconds_to_str(expected_time))
    time_diff = abs(server_time - expected_time)
    assert time_diff <= MAX_TIME_DIFF, (
        "Too much time difference (%s) between: %r: '%s' and expected: '%s" % (
        time_diff, server.box, seconds_to_str(server_time), seconds_to_str(expected_time)))

def compare_primary_box_and_server_time(env, server):
    def get_box_expected_time(env):
        return (time.time() - env.primary.date_change_time) * 1000 + env.primary.base_time
    check_server_time(env, server, get_box_expected_time)

def get_iptables_command(command):
    return ['sudo', 'iptables', command, 'OUTPUT', '-p',
            'tcp', '--dport', '37', '-j', 'DROP']

def check_iptables_inet_rule(box):
    try:
        box.host.run_command(get_iptables_command('-C'))
        return True
    except ProcessError:
        return False

def turn_inet_on_and_check_time_server(box, time_server):
    if check_iptables_inet_rule(box):
        box.host.run_command(get_iptables_command('-D'))
        box.host.run_command(
            ['nc', '-w', str(TIME_SERVER_WAIT_TIMEOUT_SEC),
             time_server.address, str(time_server.port)])

def turn_inet_off_and_check_time_server(box, time_server):
    if not check_iptables_inet_rule(box):
        box.host.run_command(get_iptables_command('-A'))
        check_time_server_is_unreachable(box, time_server)

def wait_server_inet_sync(env, server):
    '''Wait internet time server & mediaserver time synchronization'''
    start = time.time()
    while True:
        inet_time = env.time_server.get_time()
        server_time = get_server_current_time(server)
        log.debug("%r time: '%s', internet time: '%s'",
                  server.box, seconds_to_str(server_time), seconds_to_str(inet_time))
        time_diff = abs(server_time - inet_time)
        if time_diff <= MAX_TIME_DIFF:
            return
        if time.time() - start >= SYNC_TIMEOUT_SEC:
            assert False, (
                "%r: too much time difference (%s) between internet '%s:': '%s' and server: '%s'" % (
                server.box, time_diff, seconds_to_str(inet_time), seconds_to_str(server_time)))
        time.sleep(SYNC_TIMEOUT_SEC / 10.0)

def compare_inet_and_server_time(env, server):
    def get_inet_time(env):
        return env.time_server.get_time()
    check_server_time(env, server, get_inet_time)

def test_initial(env):
    check_time_server_is_unreachable(env.one.box, env.time_server)
    check_time_server_is_unreachable(env.two.box, env.time_server)
    compare_primary_box_and_server_time(env, env.primary)

def test_change_primary_server(env):
    env.secondary.rest_api.ec2.forcePrimaryTimeServer.POST(id=env.secondary.ecs_guid)
    env.primary, env.secondary = env.secondary, env.primary
    set_time_on_server(env.primary, BASE_TIME + 5*60*60)
    wait_server_box_sync(env.primary, env.primary.box)
    wait_servers_sync(env)
    compare_primary_box_and_server_time(env, env.primary)

def test_change_time_on_primary_server(env):
    set_time_on_server(env.primary, BASE_TIME + 20*60*60)
    wait_server_box_sync(env.primary, env.primary.box)
    wait_servers_sync(env)
    compare_primary_box_and_server_time(env, env.primary)

def test_change_time_on_secondary_server(env):
    set_time_on_server(env.secondary, BASE_TIME + 10*60*60)
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    compare_primary_box_and_server_time(env, env.secondary)

def test_primary_server_temporary_offline(env):
    compare_primary_box_and_server_time(env, env.secondary)
    env.primary.stop_service()
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    compare_primary_box_and_server_time(env, env.secondary)
    env.secondary.restart()
    compare_primary_box_and_server_time(env, env.secondary)
    env.primary.start_service()
    set_time_on_server(env.primary, BASE_TIME + 15*60*60)
    wait_server_box_sync(env.primary, env.primary.box)
    wait_servers_sync(env)
    compare_primary_box_and_server_time(env, env.secondary)

# This isn't specified now. Wait for a specification.
# 
# def test_secondary_server_alone(env):
#     compare_primary_box_and_server_time(env, env.secondary)
#     env.primary.service.stop()
#     env.secondary.rest_api.ec2.removeResource.POST(id=env.primary.ecs_guid)
#     servers = env.secondary.rest_api.ec2.getMediaServersEx.GET()
#     assert len(servers) == 1 and servers[0]['id'] == env.secondary.ecs_guid
#     get_server_current_time(env.secondary)
#     assert env.secondary.is_primary
#     env.primary, env.secondary = env.secondary, None
#     set_time_on_server(env.primary, BASE_TIME - 15*60*60)
#     wait_server_box_sync(env.primary, env.primary.box)    

def test_secondary_server_temporary_inet_on(env):
    # Turn on internet time on secondary box
    turn_inet_on_and_check_time_server(env.secondary.box, env.time_server)
    wait_server_inet_sync(env, env.primary)
    wait_server_inet_sync(env, env.secondary)
    # Change system time on primary box
    set_time_on_server(env.primary, BASE_TIME - 5*60*60)
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    compare_inet_and_server_time(env, env.primary)
    # Turn internet time off
    turn_inet_off_and_check_time_server(env.secondary.box, env.time_server)
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    compare_inet_and_server_time(env, env.primary)
    # Stop secondary server
    env.secondary.stop_service()
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    compare_inet_and_server_time(env, env.primary)
    env.secondary.start_service()
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    compare_inet_and_server_time(env, env.primary)
    # Restart secondary server
    env.secondary.restart()
    time.sleep(SYNC_TIMEOUT_SEC / 2.0)
    compare_inet_and_server_time(env, env.primary)
    # Stop and start both servers - so that servers forget internet time
    env.secondary.stop_service()
    env.primary.stop_service()
    time.sleep(1.0)
    env.secondary.start_service()
    env.primary.start_service()
    # Detect new primary and change its system time
    env.primary, env.secondary = get_primary_server(env)
    set_time_on_server(env.primary, BASE_TIME - 25*60*60)
    wait_server_box_sync(env.primary, env.primary.box)
    wait_servers_sync(env)
    compare_primary_box_and_server_time(env, env.secondary)
