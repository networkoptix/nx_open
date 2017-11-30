import logging
import os
import time
from datetime import datetime, timedelta

import pytest
import pytz
import requests
from requests.auth import HTTPDigestAuth

import server_api_data_generators as generator
from test_utils.rest_api import HttpError
from test_utils.server import TimePeriod
from test_utils.utils import log_list, wait_until

log = logging.getLogger(__name__)


UNEXISTENT_USER_ROLE_GUIID = '44e4161e-158e-2201-e000-000000000001'


@pytest.fixture
def server(server_factory):
    return server_factory('server')


# https://networkoptix.atlassian.net/browse/VMS-3911
def test_server_should_pick_archive_file_with_time_after_db_time(server, camera, sample_media_file):
    server.add_camera(camera)
    storage = server.storage
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
    server.rebuild_archive()
    assert expected_periods_1 == server.get_recorded_time_periods(camera)

    # stop service and add more media files to archive:
    server.stop_service()
    for st in start_times_2:
        storage.save_media_sample(camera, st, sample)
    server.start_service()

    time.sleep(10)  # servers still need some time to settle down; hope this time will be enough
    # after restart new periods must be picked:
    recorded_periods = server.get_recorded_time_periods(camera)
    assert recorded_periods != expected_periods_1, 'Server did not pick up new media archive files'
    assert expected_periods_1 + expected_periods_2 == recorded_periods


def assert_server_has_resource(server, method, **kw):
    def is_subset(subset, superset):
        '''Test that [subset] is a subset of [superset]'''
        return all(item in superset.items() for item in subset.items())
    resources = [r for r in server.rest_api.get_api_fn('GET', 'ec2', method)()
                 if is_subset(kw, r)]
    assert len(resources) != 0, "'%r' doesn't have resource '%s'" % (
        server, kw)


def assert_server_does_not_have_resource(server, method, resource_id):
    resources = server.rest_api.get_api_fn('GET', 'ec2', method)(id=resource_id)
    assert len(resources) == 0, "'%r' has unexpected resource '%s'" % (
        server, resource_id)


def assert_post_forbidden(server, method, **kw):
    with pytest.raises(HttpError) as x_info:
        server.rest_api.get_api_fn('POST', 'ec2', method)(**kw)
    assert x_info.value.status_code == 403


# https://networkoptix.atlassian.net/browse/VMS-2246
def test_create_and_remove_user_with_resource(server):
    user = generator.generate_user_data(
        user_id=1,  name="user1", email="user1@example.com",
        permissions="2432", cryptSha512Hash="", digest="",
        hash="", isAdmin=False, isEnabled=True, isLdap=False, realm="")
    server.rest_api.ec2.saveUser.POST(**user)
    expected_permissions = "GlobalViewArchivePermission|GlobalManageBookmarksPermission|0x80"
    user_resource = [generator.generate_resource_params_data(id=1, resource=user)]
    server.rest_api.ec2.setResourceParams.POST(json=user_resource)
    assert_server_has_resource(server, 'getUsers', id=user['id'], permissions=expected_permissions)
    assert_server_has_resource(server, 'getResourceParams', resourceId=user['id'], name=user_resource[0]['name'])
    server.rest_api.ec2.removeUser.POST(id=user['id'])
    assert_server_does_not_have_resource(server, 'getUsers', user['id'])
    assert_server_does_not_have_resource(server, 'getResourceParams', user['id'])


# https://networkoptix.atlassian.net/browse/VMS-3052
def test_missing_user_role(server):
    user_1 = generator.generate_user_data(user_id=1, name="user1", email="user1@example.com")
    user_2 = generator.generate_user_data(user_id=2, name="user2", email="user2@example.com",
                                          userRoleId=UNEXISTENT_USER_ROLE_GUIID)
    # Try link existing user to a missing role
    server.rest_api.ec2.saveUser.POST(**user_1)
    assert_server_has_resource(server, 'getUsers', id=user_1['id'])
    user_1_with_unexpected_role = dict(user_1, userRoleId=UNEXISTENT_USER_ROLE_GUIID)
    assert_post_forbidden(server, 'saveUser', **user_1_with_unexpected_role)

    # Try create new user to unexisting role
    assert_post_forbidden(server, 'saveUser', **user_2)

    assert_server_has_resource(server, 'getUsers', id=user_1['id'], userRoleId=user_1['userRoleId'])
    assert_server_does_not_have_resource(server, 'getUsers', user_2['id'])


# https://networkoptix.atlassian.net/browse/VMS-2904
def test_remove_child_resources(server):
    server_data = generator.generate_mediaserver_data(server_id=1)
    storage = generator.generate_storage_data(storage_id=1, parentId=server_data['id'])
    camera_1 = generator.generate_camera_data(camera_id=1, parentId=server_data['id'])
    camera_2 = generator.generate_camera_data(camera_id=2, parentId=server_data['id'])
    tested_calls = [
        ('saveMediaServer', 'getMediaServers', server_data),
        ('saveStorage', 'getStorages', storage),
        ('saveCamera', 'getCameras', camera_1),
        ('saveCamera', 'getCameras', camera_2)]
    for post_method, get_method, data in tested_calls:
        server.rest_api.get_api_fn('POST', 'ec2', post_method)(**data)
        resource_params = [generator.generate_resource_params_data(id=1, resource=data)]
        server.rest_api.ec2.setResourceParams.POST(json=resource_params)
        assert_server_has_resource(server, get_method, id=data['id'])
        assert_server_has_resource(server, 'getResourceParams', resourceId=data['id'])
    # Remove camera_2
    server.rest_api.ec2.removeResource.POST(id=camera_1['id'])
    assert_server_does_not_have_resource(server, 'getCameras', camera_1['id'])
    # Remove server and check that all server child resources have been removed
    server.rest_api.ec2.removeResource.POST(id=server_data['id'])
    for _, get_method, data in tested_calls:
        assert_server_does_not_have_resource(server, get_method, data['id'])
        assert_server_does_not_have_resource(server, 'getResourceParams', data['id'])


# https://networkoptix.atlassian.net/browse/VMS-3068
def test_http_header_server(server):
    url = server.rest_api.url + 'ec2/testConnection'
    response = requests.get(url, auth=HTTPDigestAuth(server.user, server.password))
    log.debug('%r headers: %s', server, response.headers)
    assert response.status_code == 200
    assert 'Server' in response.headers.keys(), "HTTP header 'Server' is expected"
    response = requests.get(url, auth=HTTPDigestAuth('invalid', 'invalid'))
    log.debug('%r headers: %s', server, response.headers)
    assert response.status_code == 401
    assert 'WWW-Authenticate' in response.headers.keys(), "HTTP header 'WWW-Authenticate' is expected"
    assert 'Server' not in response.headers.keys(), "Unexpected HTTP header 'Server'"


# https://networkoptix.atlassian.net/browse/VMS-3069
def test_static_vulnerability(server):
    filepath = os.path.join(server.dir, 'var', 'web', 'static', 'test.file')
    server.host.write_file(filepath, 'This is just a test file')
    url = server.rest_api.url + 'static/../../test.file'
    response = requests.get(url)
    assert response.status_code == 403


# https://networkoptix.atlassian.net/browse/VMS-7775
def test_auth_with_time_changed(timeless_server):
    url = timeless_server.rest_api.url + 'ec2/getCurrentTime'

    timeless_server.host.set_time(datetime.now(pytz.utc))
    assert wait_until(lambda: timeless_server.get_time().is_close_to(datetime.now(pytz.utc)))

    shift = timedelta(days=3)

    response = requests.get(url, auth=HTTPDigestAuth(timeless_server.user, timeless_server.password))
    authorization_header_value = response.request.headers['Authorization']
    log.info(authorization_header_value)
    response = requests.get(url, headers={'Authorization': authorization_header_value})
    response.raise_for_status()

    timeless_server.host.set_time(datetime.now(pytz.utc) + shift)
    assert wait_until(lambda: timeless_server.get_time().is_close_to(datetime.now(pytz.utc) + shift))

    response = requests.get(url, headers={'Authorization': authorization_header_value})
    assert response.status_code != 401, "Cannot authenticate after time changed on server"
    response.raise_for_status()


def test_uptime_is_monotonic(timeless_server):
    timeless_server.host.set_time(datetime.now(pytz.utc))
    first_uptime = timeless_server.rest_api.api.statistics.GET()['uptimeMs']
    new_time = timeless_server.host.set_time(datetime.now(pytz.utc) - timedelta(minutes=1))
    assert wait_until(lambda: timeless_server.get_time().is_close_to(new_time))
    second_uptime = timeless_server.rest_api.api.statistics.GET()['uptimeMs']
    assert first_uptime < second_uptime


def test_frequent_restarts(server):
    """Test for server restart REST api and functional test wrapper for it."""
    # Loop is unfolded here so that we can see which exact line is failed.
    server.restart_via_api(timeout=timedelta(seconds=10))
    server.restart_via_api(timeout=timedelta(seconds=10))
    server.restart_via_api(timeout=timedelta(seconds=10))


# https://networkoptix.atlassian.net/browse/VMS-7808
# https://networkoptix.atlassian.net/browse/VMS-7809
@pytest.mark.parametrize('path', [
    '/ec2/getFullInfoExtraSuffix', '/api/pingExtraSuffix',  # VMS-7809: Matches by prefix and returns 200.
    '/api/nonExistent', '/ec2/nonExistent'])  # VMS-7809: Redirects with 301 but not returns 404.
def test_non_existent_api_endpoints(server, path):
    auth = HTTPDigestAuth(server.user, server.password)
    response = requests.get(server.rest_api_url.rstrip('/') + path, auth=auth, allow_redirects=False)
    assert response.status_code == 404, "Expected 404 but got %r"
