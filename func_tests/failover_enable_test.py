"""Failover enable test cases

Based on testrail "Server failover".
https://networkoptix.testrail.net/index.php?/suites/view/5&group_by=cases:section_id&group_id=13&group_order=asc
"""

import logging
import time

import pytest

import server_api_data_generators as generator
from test_utils.server import MEDIASERVER_MERGE_TIMEOUT
from test_utils.utils import SimpleNamespace, datetime_utc_now, bool_to_str, str_to_bool

FAILOVER_SWITCHING_PERIOD_SEC = 4*60

log = logging.getLogger(__name__)


class Counter(object):

    def __init__(self):
        self._value = 0

    def next(self):
        self._value += 1
        return self._value


@pytest.fixture
def counter():
    return Counter()


@pytest.fixture
def env(server_factory, camera_factory, counter):
    one = server_factory('one')
    two = server_factory('two')
    three = server_factory('three')
    one.merge([two, three])
    # Create cameras
    for server in [one, two, three]:
        add_cameras_to_server(server, camera_factory, counter, 2)
    wait_until_servers_are_online([one, two, three])
    return SimpleNamespace(
        one=one,
        two=two,
        three=three
        )


def wait_until_servers_are_online(servers):
    start_time = datetime_utc_now()
    server_guids = sorted([s.ecs_guid for s in servers])
    for srv in servers:
        while True:
            online_server_guids = sorted([s['id'] for s in srv.rest_api.ec2.getMediaServersEx.GET()
                                          if s['status'] == 'Online'])
            log.debug("%r online servers: %s, system servers: %s", srv, online_server_guids, server_guids)
            if server_guids == online_server_guids:
                break
            if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
                assert server_guids == online_server_guids
            time.sleep(2.0)


def add_cameras_to_server(server, camera_factory, counter, count):
    for i in range(count):
        camera_id = counter.next()
        camera_mac = generator.generate_mac(camera_id)
        camera_name = 'Camera_%d' % camera_id
        camera = camera_factory(camera_name, camera_mac)
        camera_guid = server.add_camera(camera)
        server.rest_api.ec2.saveCameraUserAttributes.POST(
            cameraId=camera_guid, preferredServerId=server.ecs_guid)
        camera.start_streaming()


def wait_for_expected_online_cameras_on_server(server, expected_cameras):
    server_cameras = wait_for_server_cameras_become_online(server, len(expected_cameras))
    assert server_cameras == expected_cameras


def wait_for_server_cameras_become_online(server, expected_cameras_count):
    start = time.time()
    while True:
        server_cameras = sorted(
            [c['id'] for c in server.rest_api.ec2.getCamerasEx.GET()
                if c['parentId'] == server.ecs_guid and c['status'] == 'Online'])
        if len(server_cameras) == expected_cameras_count:
            return server_cameras
        if time.time() - start >= FAILOVER_SWITCHING_PERIOD_SEC:
            pytest.fail('%r has got %d online cameras after %d seconds, %d expected' %
                        (server, len(server_cameras), FAILOVER_SWITCHING_PERIOD_SEC,
                         expected_cameras_count))
        time.sleep(FAILOVER_SWITCHING_PERIOD_SEC / 10.)


def get_server_camera_ids(server):
    return sorted([c['id'] for c in server.rest_api.ec2.getCameras.GET()
                   if c['parentId'] == server.ecs_guid])


# Based on testrail C744 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/744
@pytest.mark.testcam
def test_enable_failover_on_one_server(env):
    cameras_one_initial = get_server_camera_ids(env.one)
    cameras_two_initial = get_server_camera_ids(env.two)
    env.two.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=env.two.ecs_guid,
        maxCameras=4,
        allowAutoRedundancy=True)
    env.one.stop_service()
    wait_for_expected_online_cameras_on_server(
        env.two,
        sorted(cameras_one_initial + cameras_two_initial))
    env.one.start_service()
    wait_for_expected_online_cameras_on_server(env.two, cameras_two_initial)
    wait_for_expected_online_cameras_on_server(env.one, cameras_one_initial)


@pytest.mark.testcam
def test_enable_failover_on_two_servers(env):
    cameras_one_initial = get_server_camera_ids(env.one)
    cameras_two_initial = get_server_camera_ids(env.two)
    cameras_three_initial = get_server_camera_ids(env.three)
    env.one.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=env.one.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    env.three.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=env.three.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    env.two.stop_service()
    cameras_one_failover = wait_for_server_cameras_become_online(env.one, 3)
    cameras_three_failover = wait_for_server_cameras_become_online(env.three, 3)
    assert (sorted(cameras_one_failover + cameras_three_failover) ==
            sorted(cameras_one_initial + cameras_two_initial + cameras_three_initial))
    env.two.start_service()
    wait_for_expected_online_cameras_on_server(env.one, cameras_one_initial)
    wait_for_expected_online_cameras_on_server(env.two, cameras_two_initial)
    wait_for_expected_online_cameras_on_server(env.three, cameras_three_initial)


@pytest.fixture(params=['enabled', 'disabled'])
def discovery(request):
    return request.param == 'enabled'


# Based on testrail C745 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/745
@pytest.mark.testcam
def test_failover_and_auto_discovery(server_factory, camera_factory, counter, discovery):
    one = server_factory('one')
    two = server_factory('two', setup_settings=dict(
        systemSettings=dict(autoDiscoveryEnabled=bool_to_str(discovery))))
    assert str_to_bool(two.settings['autoDiscoveryEnabled']) == discovery
    two.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=two.ecs_guid,
        maxCameras=2,
        allowAutoRedundancy=True)
    add_cameras_to_server(one, camera_factory, counter, 2)
    cameras_one = get_server_camera_ids(one)
    assert cameras_one
    one.merge([two])
    wait_until_servers_are_online([one, two])
    one.stop_service()
    wait_for_expected_online_cameras_on_server(two, cameras_one)
    one.start_service()
    wait_for_expected_online_cameras_on_server(one, cameras_one)


# Based on testrail C747 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/747
@pytest.mark.testcam
def test_max_camera_settings(server_factory, camera_factory, counter):
    one = server_factory('one')
    two = server_factory('two')
    add_cameras_to_server(one, camera_factory, counter, 2)
    add_cameras_to_server(two, camera_factory, counter, 2)
    one.merge([two])
    cameras_one_initial = get_server_camera_ids(one)
    cameras_two_initial = get_server_camera_ids(two)
    one.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=one.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    two.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=two.ecs_guid,
        maxCameras=2,
        allowAutoRedundancy=True)
    two.stop_service()
    wait_for_server_cameras_become_online(one, 3)
    one.stop_service()
    two.start_service()
    cameras_two = wait_for_server_cameras_become_online(two, 2)
    one.start_service()
    wait_for_expected_online_cameras_on_server(one, cameras_one_initial)
    wait_for_expected_online_cameras_on_server(two, cameras_two_initial)
