'''
  Failover enable test cases

  Based on testrail "Server failover"

  https://networkoptix.testrail.net/index.php?/suites/view/5&group_by=cases:section_id&group_id=13&group_order=asc
'''

import logging
import time
from operator import itemgetter

import pytest

import server_api_data_generators as generator
from test_utils.server import MEDIASERVER_MERGE_TIMEOUT
from test_utils.utils import SimpleNamespace, datetime_utc_now, bool_to_str, str_to_bool, wait_until

FAILOVER_SWITCHING_PERIOD_SEC = 4*60
START_STREAMING_PERIOD_SEC = 30

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
    wait_until_servers_are_online([one, two, three])
    # Create cameras
    one_camera_id_set = add_cameras_to_server(one, camera_factory, counter, count=2)
    two_camera_id_set = add_cameras_to_server(two, camera_factory, counter, count=2)
    three_camera_id_set = add_cameras_to_server(three, camera_factory, counter, count=2)
    return SimpleNamespace(
        one=one,
        two=two,
        three=three,
        one_camera_id_set=one_camera_id_set,
        two_camera_id_set=two_camera_id_set,
        three_camera_id_set=three_camera_id_set,
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
    camera_id_set = set()
    for i in range(count):
        camera_id = counter.next()
        camera_mac = generator.generate_mac(camera_id)
        camera_name = 'Camera_%d' % camera_id
        camera = camera_factory(camera_name, camera_mac)
        camera_guid = server.add_camera(camera)
        server.rest_api.ec2.saveCameraUserAttributes.POST(
            cameraId=camera_guid, preferredServerId=server.ecs_guid)
        camera.start_streaming()
        assert any(camera_dict for camera_dict in server.rest_api.ec2.getCameras.GET()
                       if camera_dict['parentId'] == server.ecs_guid and camera_dict['id'] == camera_guid)
        log.info('Camera is added to server %s: id=%r, mac=%r', server, camera_guid, camera_mac)
        camera_id_set.add(camera_guid)
    wait_until_cameras_are_online(server, camera_id_set, START_STREAMING_PERIOD_SEC)
    return camera_id_set


def online_camera_ids_on_server(server):
    camera_list = [camera for camera in server.rest_api.ec2.getCamerasEx.GET()
                   if camera['parentId'] == server.ecs_guid]
    for camera in sorted(camera_list, key=itemgetter('id')):
        log.info('Camera id=%r mac=%r is %r on server %s: %r', camera['id'], camera['mac'], camera['status'], server, camera)
    return set(camera['id'] for camera in camera_list if camera['status'] == 'Online')

def wait_until_cameras_are_online(server, camera_id_set, timeout_sec):
    assert wait_until(
        lambda: online_camera_ids_on_server(server) >= camera_id_set,
        timeout_sec=timeout_sec), (
        'Cameras %r did not become online on %r in %d seconds' %
            (sorted(camera_id_set), server, timeout_sec))

def wait_until_some_of_cameras_are_online(server, camera_id_set, expected_count, timeout_sec):
    assert wait_until(
        lambda: len(online_camera_ids_on_server(server) & camera_id_set) >= expected_count,
        timeout_sec=timeout_sec), (
        '%d of %d ameras %r did not become online on %r in %d seconds' %
            (expected_count, len(camera_id_set), sorted(camera_id_set), server, timeout_sec))

def get_server_camera_ids(server):
    camera_list = [c for c in server.rest_api.ec2.getCameras.GET()
                   if c['parentId'] == server.ecs_guid]
    for camera in sorted(camera_list, key=itemgetter('id')):
        log.info('Camera is on server %s: id=%r, mac=%r', server, camera['id'], camera['mac'])
    return sorted(camera['id'] for camera in camera_list)


# Based on testrail C744 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/744
@pytest.mark.testcam
def test_enable_failover_on_one_server(env):
    env.two.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=env.two.ecs_guid,
        maxCameras=4,
        allowAutoRedundancy=True)
    # stop server one; all it's cameras must go to server two
    env.one.stop_service()
    wait_until_cameras_are_online(env.two, env.one_camera_id_set | env.two_camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)
    # start server one again; all it's cameras must go back to him
    env.one.start_service()
    wait_until_cameras_are_online(env.one, env.one_camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)
    wait_until_cameras_are_online(env.two, env.two_camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)


@pytest.mark.testcam
def test_enable_failover_on_two_servers(env):
    env.one.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=env.one.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    env.three.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=env.three.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    # stop server two; one of it's 2 cameras must go to server one, another - to server three:
    env.two.stop_service()
    wait_until_some_of_cameras_are_online(env.one, env.one_camera_id_set | env.two_camera_id_set, 3, FAILOVER_SWITCHING_PERIOD_SEC)
    assert len(online_camera_ids_on_server(env.one)) == 3, (
        'Server "one" maxCameras limit (3) does not work - it got more than 3 cameras')
    wait_until_some_of_cameras_are_online(env.three, env.three_camera_id_set | env.two_camera_id_set, 3, FAILOVER_SWITCHING_PERIOD_SEC)
    assert len(online_camera_ids_on_server(env.three)) == 3, (
        'Server "three" maxCameras limit (3) does not work - it got more than 3 cameras')
    assert (online_camera_ids_on_server(env.one) | online_camera_ids_on_server(env.three) ==
                env.one_camera_id_set | env.two_camera_id_set | env.three_camera_id_set)
    # start server two back; cameras must return to their original owners
    env.two.start_service()
    wait_until_cameras_are_online(env.one, env.one_camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)
    wait_until_cameras_are_online(env.two, env.two_camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)
    wait_until_cameras_are_online(env.three, env.three_camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)


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
    camera_id_set = add_cameras_to_server(one, camera_factory, counter, count=2)
    assert camera_id_set <= set(get_server_camera_ids(one))
    wait_until_cameras_are_online(one, camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)
    one.merge([two])
    wait_until_servers_are_online([one, two])
    wait_until_cameras_are_online(one, camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)
    one.stop_service()
    wait_until_cameras_are_online(two, camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)
    one.start_service()
    wait_until_cameras_are_online(one, camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)


# Based on testrail C747 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/747
@pytest.mark.testcam
def test_max_camera_settings(server_factory, camera_factory, counter):
    one = server_factory('one')
    two = server_factory('two')
    one_camera_id_set = add_cameras_to_server(one, camera_factory, counter, count=2)
    two_camera_id_set = add_cameras_to_server(two, camera_factory, counter, count=2)
    one.merge([two])
    one.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=one.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    two.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=two.ecs_guid,
        maxCameras=2,
        allowAutoRedundancy=True)
    # stop server two; exactly one camera from server two must go to server one
    two.stop_service()
    wait_until_some_of_cameras_are_online(one, one_camera_id_set | two_camera_id_set, 3, FAILOVER_SWITCHING_PERIOD_SEC)
    assert len(online_camera_ids_on_server(one) & two_camera_id_set) == 1, (
        'Server "one" maxCameras limit (3) does not work - it got more than 1 cameras from server two')
    # stop server one, and start two; no additional cameras must go to server two
    one.stop_service()
    two.start_service()
    wait_until_cameras_are_online(two, two_camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)
    assert len(online_camera_ids_on_server(two)) == 2, (
        'Server "two" maxCameras limit (2) does not work - it got cameras from server one, while already has 2 own cameras')
    # start server one again; all cameras must go back to their original owners
    one.start_service()
    wait_until_cameras_are_online(one, one_camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)
    wait_until_cameras_are_online(two, two_camera_id_set, FAILOVER_SWITCHING_PERIOD_SEC)
