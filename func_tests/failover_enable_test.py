"""Failover enable test cases

Based on testrail "Server failover".
https://networkoptix.testrail.net/index.php?/suites/view/5&group_by=cases:section_id&group_id=13&group_order=asc
"""

import logging
import time
from collections import namedtuple
from operator import itemgetter

import pytest

import server_api_data_generators as generator
from test_utils.server import MEDIASERVER_MERGE_TIMEOUT
from test_utils.utils import datetime_utc_now, bool_to_str, str_to_bool, wait_until


CAMERA_SWITCHING_PERIOD_SEC = 4*60

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


ServerRec = namedtuple('ServerRec', 'server camera_mac_set')

# Each server can have own copy of each auto-discovered cameras until they completely merge.
# To catch exact moment when servers merge complete, we first wait until each server discover all cameras.
# After that we merge servers and wait until preferred cameras settle down on destined servers.

def create_cameras_and_servers(server_factory, camera_factory, counter, server_name_list):
    server_count = len(server_name_list)
    # we always use 2 cameras per server
    all_camera_mac_set = set(create_cameras(camera_factory, counter, count=server_count*2))
    server_list = [create_server(server_factory, server_name, all_camera_mac_set)
                       for server_name in server_name_list]
    server_list[0].merge(server_list[1:])
    wait_until_servers_are_online(server_list)
    server_rec_list = [ServerRec(server, set(sorted(all_camera_mac_set)[idx*2 : idx*2 + 2]))
                           for idx, server in enumerate(server_list)]
    for rec in server_rec_list:
        attach_cameras_to_server(rec.server, rec.camera_mac_set)
    for rec in server_rec_list:
        wait_until_cameras_on_server_reduced_to(rec.server, rec.camera_mac_set)
    return server_rec_list

def create_server(server_factory, name, all_camera_mac_set, setup_settings=None):
    server = server_factory.get(name, setup_settings=setup_settings)
    wait_until_cameras_are_online(server, all_camera_mac_set)
    return server

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


def create_cameras(camera_factory, counter, count):
    for i in range(count):
        camera_num = counter.next()
        camera_mac = generator.generate_mac(camera_num)
        camera_name = 'Camera_%d' % camera_num
        camera = camera_factory(camera_name, camera_mac)
        camera.start_streaming()
        log.info('Camera is created: mac=%r', camera.mac_addr)
        yield camera.mac_addr

def attach_cameras_to_server(server, camera_mac_list):
    camera_mac_to_id = {camera['mac']: camera['id'] for camera in server.rest_api.ec2.getCameras.GET()}
    for camera_mac in camera_mac_list:
        camera_id = camera_mac_to_id[camera_mac]
        server.rest_api.ec2.saveCameraUserAttributes.POST(
            cameraId=camera_id, preferredServerId=server.ecs_guid)
        log.info('Camera is set as preferred for server %s: mac=%r, id=%r', server, camera_mac, camera_id)

def online_camera_macs_on_server(server):
    camera_list = [camera for camera in server.rest_api.ec2.getCamerasEx.GET()
                   if camera['parentId'] == server.ecs_guid]
    for camera in sorted(camera_list, key=itemgetter('mac')):
        log.info('Camera mac=%r id=%r is %r on server %s: %r', camera['mac'], camera['id'], camera['status'], server, camera)
    return set(camera['mac'] for camera in camera_list if camera['status'] == 'Online')

def wait_until_cameras_on_server_reduced_to(server, camera_mac_set):
    assert wait_until(
        lambda: online_camera_macs_on_server(server) == camera_mac_set,
        timeout_sec=CAMERA_SWITCHING_PERIOD_SEC), (
            'Cameras on server %s were not reduced to %r in %d seconds' %
            (server, sorted(camera_mac_set), CAMERA_SWITCHING_PERIOD_SEC))

def wait_until_camera_count_is_online(server, camera_count):
    assert wait_until(
        lambda: len(online_camera_macs_on_server(server)) >= camera_count,
        timeout_sec=CAMERA_SWITCHING_PERIOD_SEC), (
        '%d cameras did not become online on %r in %d seconds' %
            (camera_count, server, CAMERA_SWITCHING_PERIOD_SEC))

def wait_until_cameras_are_online(server, camera_mac_set):
    assert wait_until(
        lambda: online_camera_macs_on_server(server) >= camera_mac_set,
        timeout_sec=CAMERA_SWITCHING_PERIOD_SEC), (
        'Cameras %r did not become online on %r in %d seconds' %
            (sorted(camera_mac_set), server, CAMERA_SWITCHING_PERIOD_SEC))

def get_server_camera_macs(server):
    camera_list = [c for c in server.rest_api.ec2.getCameras.GET()
                   if c['parentId'] == server.ecs_guid]
    for camera in sorted(camera_list, key=itemgetter('id')):
        log.info('Camera is on server %s: id=%r, mac=%r', server, camera['id'], camera['mac'])
    return sorted(camera['id'] for camera in camera_list)


# Based on testrail C744 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/744
@pytest.mark.testcam
def test_enable_failover_on_one_server(server_factory, camera_factory, counter):
    one, two, three = create_cameras_and_servers(server_factory, camera_factory, counter, ['one', 'two', 'three'])
    two.server.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=two.server.ecs_guid,
        maxCameras=4,
        allowAutoRedundancy=True)
    # stop server one; all it's cameras must go to server two
    one.server.stop_service()
    wait_until_camera_count_is_online(two.server, 4)
    # start server one again; all it's cameras must go back to him
    one.server.start_service()
    wait_until_camera_count_is_online(one.server, 2)
    wait_until_camera_count_is_online(two.server, 2)


@pytest.mark.testcam
def test_enable_failover_on_two_servers(server_factory, camera_factory, counter):
    one, two, three = create_cameras_and_servers(server_factory, camera_factory, counter, ['one', 'two', 'three'])
    one.server.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=one.server.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    three.server.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=three.server.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    # stop server two; one of it's 2 cameras must go to server one, another - to server three:
    two.server.stop_service()
    wait_until_camera_count_is_online(one.server, 3)
    assert len(online_camera_macs_on_server(one.server)) == 3, (
        'Server "one" maxCameras limit (3) does not work - it got more than 3 cameras')
    wait_until_camera_count_is_online(three.server, 3)
    assert len(online_camera_macs_on_server(three.server)) == 3, (
        'Server "three" maxCameras limit (3) does not work - it got more than 3 cameras')
    # start server two back; cameras must return to their original owners
    two.server.start_service()
    wait_until_camera_count_is_online(two.server, 2)


@pytest.fixture(params=['enabled', 'disabled'])
def discovery(request):
    return request.param == 'enabled'


# Based on testrail C745 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/745
@pytest.mark.testcam
def test_failover_and_auto_discovery(server_factory, camera_factory, counter, discovery):
    camera_mac_set = set(create_cameras(camera_factory, counter, count=2))
    one = create_server(server_factory, 'one', camera_mac_set)
    two = create_server(server_factory, 'two', set(), setup_settings=dict(
        systemSettings=dict(autoDiscoveryEnabled=bool_to_str(discovery))))
    assert str_to_bool(two.settings['autoDiscoveryEnabled']) == discovery
    attach_cameras_to_server(one, camera_mac_set)
    one.merge([two])
    wait_until_servers_are_online([one, two])
    wait_until_cameras_on_server_reduced_to(two, set())  # recheck there are no cameras on server two
    two.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=two.ecs_guid,
        maxCameras=2,
        allowAutoRedundancy=True)
    # stop server one - all cameras must go to server two
    one.stop_service()
    wait_until_camera_count_is_online(two, 2)
    # start server one again - all cameras must go back
    one.start_service()
    wait_until_cameras_are_online(one, camera_mac_set)


# Based on testrail C747 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/747
@pytest.mark.testcam
def test_max_camera_settings(server_factory, camera_factory, counter):
    one, two = create_cameras_and_servers(server_factory, camera_factory, counter, ['one', 'two'])
    one.server.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=one.server.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    two.server.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=two.server.ecs_guid,
        maxCameras=2,
        allowAutoRedundancy=True)
    # stop server two; exactly one camera from server two must go to server one
    two.server.stop_service()
    wait_until_camera_count_is_online(one.server, 3)
    assert len(online_camera_macs_on_server(one.server)) == 3, (
        'Server "one" maxCameras limit (3) does not work - it got more than 3 cameras')
    # stop server one, and start two; no additional cameras must go to server two
    one.server.stop_service()
    two.server.start_service()
    wait_until_camera_count_is_online(two.server, 2)
    assert len(online_camera_macs_on_server(two.server)) == 2, (
        'Server "two" maxCameras limit (2) does not work - it got cameras from server one, while already has 2 own cameras')
    # start server one again; all cameras must go back to their original owners
    one.server.start_service()
    wait_until_camera_count_is_online(one.server, 2)
    wait_until_camera_count_is_online(two.server, 2)
