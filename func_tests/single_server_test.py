from datetime import datetime, timedelta
import os
import time
import pytest
import pytz
import requests
import logging
from test_utils.utils import log_list
from test_utils.server import TimePeriod
import server_api_data_generators as generator
from test_utils.server_rest_api import HttpError
from requests.auth import HTTPDigestAuth

log = logging.getLogger(__name__)


UNEXISTENT_USER_ROLE_GUIID = '44e4161e-158e-2201-e000-000000000001'


@pytest.fixture
def env(env_builder, server):
    server = server()
    return env_builder(server=server)


# https://networkoptix.atlassian.net/browse/VMS-3911
def test_server_should_pick_archive_file_with_time_after_db_time(env, camera, sample_media_file):
    env.server.add_camera(camera)
    storage = env.server.storage
    sample = sample_media_file

    start_times_1 = []
    start_times_1.append(datetime(2017, 1, 27, tzinfo=pytz.utc))
    start_times_1.append(start_times_1[-1] + sample.duration + timedelta(minutes=1))
    start_times_2 = []
    start_times_2.append(start_times_1[-1] + sample.duration + timedelta(minutes=1))
    start_times_2.append(start_times_2[-1] + sample.duration + timedelta(minutes=1))
    expected_periods_1 = [
        TimePeriod(start_times_1[0], sample.duration),
        TimePeriod(start_times_1[1], sample.duration),
        ]
    expected_periods_2 = [
        TimePeriod(start_times_2[0], sample.duration),
        TimePeriod(start_times_2[1], sample.duration),
        ]
    log_list('Start times 1', start_times_1)
    log_list('Start times 2', start_times_2)
    log_list('Expected periods 1', expected_periods_1)
    log_list('Expected periods 2', expected_periods_2)

    for st in start_times_1:
        storage.save_media_sample(camera, st, sample)
    env.server.rebuild_archive()
    assert expected_periods_1 == env.server.get_recorded_time_periods(camera)

    # stop service and add more media files to archive:
    env.server.stop_service()
    for st in start_times_2:
        storage.save_media_sample(camera, st, sample)
    env.server.start_service()

    time.sleep(10)  # servers still need some time to settle down; hope this time will be enough
    # after restart new periods must be picked:
    recorded_periods = env.server.get_recorded_time_periods(camera)
    assert recorded_periods != expected_periods_1, 'Server did not pick up new media archive files'
    assert expected_periods_1 + expected_periods_2 == recorded_periods


def assert_server_has_resource(env, method, **kw):
    def is_subset(subset, superset):
        '''Test that [subset] is a subset of [superset]'''
        return all(item in superset.items() for item in subset.items())
    resources = [r for r in env.server.rest_api.get_api_fn('GET', 'ec2', method)()
                 if is_subset(kw, r)]
    assert len(resources) != 0, "'%r' doesn't have resource '%s'" % (
        env.server, kw)


def assert_server_does_not_have_resource(env, method, resource_id):
    resources = env.server.rest_api.get_api_fn('GET', 'ec2', method)(id=resource_id)
    assert len(resources) == 0, "'%r' has unexpected resource '%s'" % (
        env.server, resource_id)


def assert_post_forbidden(env, method, **kw):
    with pytest.raises(HttpError) as x_info:
        env.server.rest_api.get_api_fn('POST', 'ec2', method)(**kw)
    assert x_info.value.status_code == 403


# https://networkoptix.atlassian.net/browse/VMS-2246
def test_create_and_remove_user_with_resource(env):
    user = generator.generate_user_data(
        user_id=1,  name="user1", email="user1@example.com",
        permissions="2432", cryptSha512Hash="", digest="",
        hash="", isAdmin=False, isEnabled=True, isLdap=False, realm="")
    env.server.rest_api.ec2.saveUser.POST(**user)
    expected_permissions = "GlobalViewArchivePermission|GlobalManageBookmarksPermission|0x80"
    user_resource = [generator.generate_resource_params_data(id=1, resource=user)]
    env.server.rest_api.ec2.setResourceParams.POST(json=user_resource)
    assert_server_has_resource(env, 'getUsers', id=user['id'], permissions=expected_permissions)
    assert_server_has_resource(env, 'getResourceParams', resourceId=user['id'], name=user_resource[0]['name'])
    env.server.rest_api.ec2.removeUser.POST(id=user['id'])
    assert_server_does_not_have_resource(env, 'getUsers', user['id'])
    assert_server_does_not_have_resource(env, 'getResourceParams', user['id'])


# https://networkoptix.atlassian.net/browse/VMS-3052
def test_missing_user_role(env):
    user_1 = generator.generate_user_data(user_id=1, name="user1", email="user1@example.com")
    user_2 = generator.generate_user_data(user_id=2, name="user2", email="user2@example.com",
                                          userRoleId=UNEXISTENT_USER_ROLE_GUIID)
    # Try link existing user to a missing role
    env.server.rest_api.ec2.saveUser.POST(**user_1)
    assert_server_has_resource(env, 'getUsers', id=user_1['id'])
    user_1_with_unexpected_role = dict(user_1, userRoleId=UNEXISTENT_USER_ROLE_GUIID)
    assert_post_forbidden(env, 'saveUser', **user_1_with_unexpected_role)

    # Try create new user to unexisting role
    assert_post_forbidden(env, 'saveUser', **user_2)

    assert_server_has_resource(env, 'getUsers', id=user_1['id'], userRoleId=user_1['userRoleId'])
    assert_server_does_not_have_resource(env, 'getUsers', user_2['id'])


# https://networkoptix.atlassian.net/browse/VMS-2904
def test_remove_child_resources(env):
    server = generator.generate_mediaserver_data(server_id=1)
    storage = generator.generate_storage_data(storage_id=1, parentId=server['id'])
    camera_1 = generator.generate_camera_data(camera_id=1, parentId=server['id'])
    camera_2 = generator.generate_camera_data(camera_id=2, parentId=server['id'])
    tested_calls = [
        ('saveMediaServer', 'getMediaServers', server),
        ('saveStorage', 'getStorages', storage),
        ('saveCamera', 'getCameras', camera_1),
        ('saveCamera', 'getCameras', camera_2)]
    for post_method, get_method, data in tested_calls:
        env.server.rest_api.get_api_fn('POST', 'ec2', post_method)(**data)
        resource_params = [generator.generate_resource_params_data(id=1, resource=data)]
        env.server.rest_api.ec2.setResourceParams.POST(json=resource_params)
        assert_server_has_resource(env, get_method, id=data['id'])
        assert_server_has_resource(env, 'getResourceParams', resourceId=data['id'])
    # Remove camera_2
    env.server.rest_api.ec2.removeResource.POST(id=camera_1['id'])
    assert_server_does_not_have_resource(env, 'getCameras', camera_1['id'])
    # Remove server and check that all server child resources have been removed
    env.server.rest_api.ec2.removeResource.POST(id=server['id'])
    for _, get_method, data in tested_calls:
        assert_server_does_not_have_resource(env, get_method, data['id'])
        assert_server_does_not_have_resource(env, 'getResourceParams', data['id'])


# https://networkoptix.atlassian.net/browse/VMS-3068
def test_http_header_server(env):
    url = env.server.rest_api.url + 'ec2/testConnection'
    response = requests.get(url, auth=HTTPDigestAuth(env.server.user, env.server.password))
    log.debug('%r headers: %s', env.server, response.headers)
    assert response.status_code == 200
    assert 'Server' in response.headers.keys(), "HTTP header 'Server' is expected"
    response = requests.get(url, auth=HTTPDigestAuth('invalid', 'invalid'))
    log.debug('%r headers: %s', env.server, response.headers)
    assert response.status_code == 401
    assert 'WWW-Authenticate' in response.headers.keys(), "HTTP header 'WWW-Authenticate' is expected"
    assert 'Server' not in response.headers.keys(), "Unexpected HTTP header 'Server'"


# https://networkoptix.atlassian.net/browse/VMS-3069
def test_static_vulnerability(env):
    filepath = os.path.join(env.server.dir, 'var', 'web', 'static', 'test.file')
    env.server.box.host.write_file(filepath, 'This is just a test file')
    url = env.server.rest_api.url + 'static/../../test.file'
    response = requests.get(url)
    assert response.status_code == 403
