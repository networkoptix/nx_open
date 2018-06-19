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
from framework.api_shortcuts import get_server_id, get_system_settings
from framework.installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from framework.merging import merge_systems, setup_local_system
from framework.utils import bool_to_str, datetime_utc_now, str_to_bool
from framework.waiting import wait_for_true
from framework.pool import ClosingPool

CAMERA_SWITCHING_PERIOD_SEC = 4*60

_logger = logging.getLogger(__name__)


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
def layout():
    return {
        'networks':
        {
            '10.254.0.0/28':
            {
                'one': None,
                'two': None,
                'three': None
            }
        }
    }


@pytest.fixture
def three_mediaservers(network, mediaserver_factory):
    allocated_mediaservers = []
    with ClosingPool(mediaserver_factory.allocated_mediaserver, network, None) as mediaservers:
        for name in ['one', 'two', 'three']:
            mediaserver = mediaservers.get(name)
            mediaserver.start()
            allocated_mediaservers.append(mediaserver)
        yield allocated_mediaservers


ServerRec = namedtuple('ServerRec', 'server camera_mac_set')


# Each server can have own copy of each auto-discovered cameras until they completely merge.
# To catch exact moment when servers merge complete, we first wait until each server discover all cameras.
# After that we merge servers and wait until preferred cameras settle down on destined servers.
def create_cameras_and_setup_servers(servers, camera_factory, counter):
    server_count = len(servers)
    # we always use 2 cameras per server
    all_camera_mac_set = set(create_cameras(camera_factory, counter, count=server_count*2))
    for server in servers:
        setup_server(server, all_camera_mac_set)
    for server in servers[1:]:
        merge_systems(servers[0], server)
    wait_until_servers_are_online(servers)
    server_rec_list = [
        ServerRec(server, set(sorted(all_camera_mac_set)[idx * 2: idx * 2 + 2]))
        for idx, server in enumerate(servers)]
    for rec in server_rec_list:
        attach_cameras_to_server(rec.server, rec.camera_mac_set)
    for rec in server_rec_list:
        wait_until_cameras_on_server_reduced_to(rec.server, rec.camera_mac_set)


def setup_server(server, all_camera_mac_set, system_settings=None):
    setup_local_system(server, system_settings or {})
    wait_until_cameras_are_online(server, all_camera_mac_set)


def wait_until_servers_are_online(servers):
    start_time = datetime_utc_now()
    server_guids = sorted([get_server_id(s.api) for s in servers])
    for srv in servers:
        while True:
            online_server_guids = sorted([s['id'] for s in srv.api.ec2.getMediaServersEx.GET()
                                          if s['status'] == 'Online'])
            _logger.debug("%r online servers: %s, system servers: %s", srv, online_server_guids, server_guids)
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
        _logger.info('Camera is created: mac=%r', camera.mac_addr)
        yield camera.mac_addr


def attach_cameras_to_server(server, camera_mac_list):
    camera_mac_to_id = {camera['mac']: camera['id'] for camera in server.api.ec2.getCameras.GET()}
    for camera_mac in camera_mac_list:
        camera_id = camera_mac_to_id[camera_mac]
        server.api.ec2.saveCameraUserAttributes.POST(
            cameraId=camera_id, preferredServerId=get_server_id(server.api))
        _logger.info('Camera is set as preferred for server %s: mac=%r, id=%r', server, camera_mac, camera_id)


def online_camera_macs_on_server(server):
    camera_list = [camera for camera in server.api.ec2.getCamerasEx.GET()
                   if camera['parentId'] == get_server_id(server.api)]
    for camera in sorted(camera_list, key=itemgetter('mac')):
        _logger.info(
            'Camera mac=%r id=%r is %r on server %s: %r',
            camera['mac'], camera['id'], camera['status'], server, camera)
    return set(camera['mac'] for camera in camera_list if camera['status'] == 'Online')


def wait_until_cameras_on_server_reduced_to(server, camera_mac_set):
    wait_for_true(
        lambda: online_camera_macs_on_server(server) == camera_mac_set,
        'cameras on server {} were not reduced to {} in {:d} seconds'.format(
            server, sorted(camera_mac_set), CAMERA_SWITCHING_PERIOD_SEC),
        timeout_sec=CAMERA_SWITCHING_PERIOD_SEC)


def wait_until_camera_count_is_online(server, camera_count):
    wait_for_true(
        lambda: len(online_camera_macs_on_server(server)) >= camera_count,
        '{:d} cameras did not become online on {} in {:d} seconds'.format(
            camera_count, server, CAMERA_SWITCHING_PERIOD_SEC),
        timeout_sec=CAMERA_SWITCHING_PERIOD_SEC)


def wait_until_cameras_are_online(server, camera_mac_set):
    wait_for_true(
        lambda: online_camera_macs_on_server(server) >= camera_mac_set,
        'Cameras {} did not become online on {} in {:d} seconds'.format(
            sorted(camera_mac_set), server, CAMERA_SWITCHING_PERIOD_SEC),
        timeout_sec=CAMERA_SWITCHING_PERIOD_SEC)


def get_server_camera_macs(server):
    server_guid = get_server_id(server.api)
    camera_list = [c for c in server.api.ec2.getCameras.GET()
                   if c['parentId'] == server_guid]
    for camera in sorted(camera_list, key=itemgetter('id')):
        _logger.info('Camera is on server %s: id=%r, mac=%r', server, camera['id'], camera['mac'])
    return sorted(camera['id'] for camera in camera_list)


# Based on testrail C744 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/744
@pytest.mark.testcam
def test_enable_failover_on_one_server(three_mediaservers, camera_factory, counter):
    one, two, three = three_mediaservers
    create_cameras_and_setup_servers([one, two, three], camera_factory, counter)
    two.api.ec2.saveMediaServerUserAttributes.POST(
        serverId=get_server_id(two.api),
        maxCameras=4,
        allowAutoRedundancy=True)
    # stop server one; all it's cameras must go to server two
    one.stop()
    wait_until_camera_count_is_online(two, 4)
    # start server one again; all it's cameras must go back to him
    one.start()
    wait_until_camera_count_is_online(one, 2)
    wait_until_camera_count_is_online(two, 2)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()
    assert not three.installation.list_core_dumps()


@pytest.mark.testcam
def test_enable_failover_on_two_servers(three_mediaservers, camera_factory, counter):
    one, two, three = three_mediaservers
    create_cameras_and_setup_servers([one, two, three], camera_factory, counter)
    one.api.ec2.saveMediaServerUserAttributes.POST(
        serverId=get_server_id(one.api),
        maxCameras=3,
        allowAutoRedundancy=True)
    three.api.ec2.saveMediaServerUserAttributes.POST(
        serverId=get_server_id(three.api),
        maxCameras=3,
        allowAutoRedundancy=True)
    # stop server two; one of it's 2 cameras must go to server one, another - to server three:
    two.stop()
    wait_until_camera_count_is_online(one, 3)
    assert len(online_camera_macs_on_server(one)) == 3, (
        'Mediaserver "one" maxCameras limit (3) does not work - it got more than 3 cameras')
    wait_until_camera_count_is_online(three, 3)
    assert len(online_camera_macs_on_server(three)) == 3, (
        'Mediaserver "three" maxCameras limit (3) does not work - it got more than 3 cameras')
    # start server two back; cameras must return to their original owners
    two.start()
    wait_until_camera_count_is_online(two, 2)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()
    assert not three.installation.list_core_dumps()


@pytest.fixture(params=['enabled', 'disabled'])
def discovery(request):
    return request.param == 'enabled'


# Based on testrail C745 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/745
@pytest.mark.testcam
def test_failover_and_auto_discovery(two_clean_mediaservers, camera_factory, counter, discovery):
    camera_mac_set = set(create_cameras(camera_factory, counter, count=2))
    one, two = two_clean_mediaservers
    setup_server(one, camera_mac_set)
    setup_server(two, set(), system_settings=dict(autoDiscoveryEnabled=bool_to_str(discovery)))
    assert str_to_bool(get_system_settings(two.api)['autoDiscoveryEnabled']) == discovery
    attach_cameras_to_server(one, camera_mac_set)
    merge_systems(one, two)
    wait_until_servers_are_online([one, two])
    wait_until_cameras_on_server_reduced_to(two, set())  # recheck there are no cameras on server two
    two.api.ec2.saveMediaServerUserAttributes.POST(
        serverId=get_server_id(two.api),
        maxCameras=2,
        allowAutoRedundancy=True)
    # stop server one - all cameras must go to server two
    one.stop()
    wait_until_camera_count_is_online(two, 2)
    # start server one again - all cameras must go back
    one.start()
    wait_until_cameras_are_online(one, camera_mac_set)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()


# Based on testrail C747 Check "Enable failover" for server
# https://networkoptix.testrail.net/index.php?/cases/view/747
@pytest.mark.testcam
def test_max_camera_settings(two_clean_mediaservers, camera_factory, counter):
    one, two = two_clean_mediaservers
    create_cameras_and_setup_servers([one, two], camera_factory, counter)
    one.api.ec2.saveMediaServerUserAttributes.POST(
        serverId=get_server_id(one.api),
        maxCameras=3,
        allowAutoRedundancy=True)
    two.api.ec2.saveMediaServerUserAttributes.POST(
        serverId=get_server_id(two.api),
        maxCameras=2,
        allowAutoRedundancy=True)
    # stop server two; exactly one camera from server two must go to server one
    two.stop()
    wait_until_camera_count_is_online(one, 3)
    assert len(online_camera_macs_on_server(one)) == 3, (
        'Mediaserver "one" maxCameras limit (3) does not work - it got more than 3 cameras')
    # stop server one, and start two; no additional cameras must go to server two
    one.stop()
    two.start()
    wait_until_camera_count_is_online(two, 2)
    assert len(online_camera_macs_on_server(two)) == 2, (
        'Mediaserver "two" maxCameras limit (2) does not work - '
        'it got cameras from server one, while already has 2 own cameras')
    # start server one again; all cameras must go back to their original owners
    one.start()
    wait_until_camera_count_is_online(one, 2)
    wait_until_camera_count_is_online(two, 2)

    assert not one.installation.list_core_dumps()
    assert not two.installation.list_core_dumps()
