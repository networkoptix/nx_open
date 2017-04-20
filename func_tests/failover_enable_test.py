'''
  Failover enable test cases

  Based on testrail C744 Check "Enable failover" for server

  https://networkoptix.testrail.net/index.php?/cases/view/744
'''

import pytest
import time
import server_api_data_generators as generator


FAILOVER_SWITCHING_PERIOD_SEC = 4*60


class SeedCounter(object):

    _seed = 0

    def next(self):
        self._seed += 1
        return self._seed


@pytest.fixture
def seed():
    return SeedCounter()


@pytest.fixture
def env(env_builder, server, camera_factory, seed):
    one = server()
    two = server()
    three = server()
    built_env = env_builder(merge_servers=[one, two, three],
                            one=one, two=two, three=three)
    # Create cameras
    for srv in built_env.servers.values():
        add_cameras_to_env(srv, camera_factory, seed, 2)
    return built_env


def add_cameras_to_env(server, camera_factory, seed, count):
    for i in range(count):
        camera_id = seed.next()
        camera_mac = generator.generate_mac(camera_id)
        camera_name = 'Camera_%d' % camera_id
        camera = camera_factory(camera_name, camera_mac)
        camera_guid = server.add_camera(camera)
        server.rest_api.ec2.saveCameraUserAttributes.POST(
            cameraId=camera_guid, preferredServerId=server.ecs_guid)
        camera.start_streaming()


def wait_for_expected_online_cameras_on_server(server, expected_cameras):
    server_cameras = wait_for_server_online_cameras(server, len(expected_cameras))
    assert server_cameras == expected_cameras


def wait_for_server_online_cameras(server, expected_cameras_count):
    start = time.time()
    while True:
        server_cameras = sorted(
            [c['id'] for c in server.rest_api.ec2.getCamerasEx.GET()
                if c['parentId'] == server.ecs_guid and c['status'] == 'Online'])
        if len(server_cameras) == expected_cameras_count:
            return server_cameras
        if time.time() - start >= FAILOVER_SWITCHING_PERIOD_SEC:
            pytest.fail('%r has got only %d online cameras after %d seconds, %d expected' %
                        (server, len(server_cameras), FAILOVER_SWITCHING_PERIOD_SEC,
                         expected_cameras_count))
        time.sleep(FAILOVER_SWITCHING_PERIOD_SEC / 10.)


def get_server_cameras(server):
    return sorted([c['id'] for c in server.rest_api.ec2.getCameras.GET()
                   if c['parentId'] == server.ecs_guid])


def test_enable_failover_on_one_server(env):
    cameras_one_initial = get_server_cameras(env.one)
    cameras_two_initial = get_server_cameras(env.two)
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


def test_enable_failover_on_two_servers(env):
    cameras_one_initial = get_server_cameras(env.one)
    cameras_two_initial = get_server_cameras(env.two)
    cameras_three_initial = get_server_cameras(env.three)
    env.one.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=env.one.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    env.three.rest_api.ec2.saveMediaServerUserAttributes.POST(
        serverId=env.three.ecs_guid,
        maxCameras=3,
        allowAutoRedundancy=True)
    env.two.stop_service()
    cameras_one_failover = wait_for_server_online_cameras(env.one, 3)
    cameras_three_failover = wait_for_server_online_cameras(env.three, 3)
    assert (sorted(cameras_one_failover + cameras_three_failover) ==
            sorted(cameras_one_initial + cameras_two_initial + cameras_three_initial))
    env.two.start_service()
    wait_for_expected_online_cameras_on_server(env.one, cameras_one_initial)
    wait_for_expected_online_cameras_on_server(env.two, cameras_two_initial)
    wait_for_expected_online_cameras_on_server(env.three, cameras_three_initial)
