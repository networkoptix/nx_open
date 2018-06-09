import logging
import time
import warnings
from datetime import datetime, timedelta

import pytest
import pytz
import requests
import urllib3.exceptions
from requests.auth import HTTPDigestAuth

import server_api_data_generators as generator
from framework.api_shortcuts import get_time
from framework.installation.mediaserver import TimePeriod
from framework.rest_api import HttpError, REST_API_TIMEOUT_SEC
from framework.utils import log_list
from framework.waiting import wait_for_true
from framework.timeless_mediaserver import timeless_mediaserver

_logger = logging.getLogger(__name__)


UNEXISTENT_USER_ROLE_GUIID = '44e4161e-158e-2201-e000-000000000001'


def test_saved_media_should_appear_after_archive_is_rebuilt(one_running_mediaserver, camera, sample_media_file):
    server = one_running_mediaserver
    start_time = datetime(2017, 3, 27, tzinfo=pytz.utc)
    server.add_camera(camera)
    server.storage.save_media_sample(camera, start_time, sample_media_file)
    server.rebuild_archive()
    assert (one_running_mediaserver.get_recorded_time_periods(camera) ==
            [TimePeriod(start_time, sample_media_file.duration)])


# https://networkoptix.atlassian.net/browse/VMS-3911
def test_server_should_pick_archive_file_with_time_after_db_time(one_running_mediaserver, camera, sample_media_file):
    one_running_mediaserver.add_camera(camera)
    storage = one_running_mediaserver.storage
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
    one_running_mediaserver.rebuild_archive()
    assert expected_periods_1 == one_running_mediaserver.get_recorded_time_periods(camera)

    # stop service and add more media files to archive:
    one_running_mediaserver.stop()
    for st in start_times_2:
        storage.save_media_sample(camera, st, sample)
    one_running_mediaserver.start()

    time.sleep(10)  # servers still need some time to settle down; hope this time will be enough
    # after restart new periods must be picked:
    recorded_periods = one_running_mediaserver.get_recorded_time_periods(camera)
    assert recorded_periods != expected_periods_1, 'Mediaserver did not pick up new media archive files'
    assert expected_periods_1 + expected_periods_2 == recorded_periods

    assert not one_running_mediaserver.installation.list_core_dumps()


def assert_server_has_resource(server, method, **kw):
    def is_subset(subset, superset):
        return all(item in superset.items() for item in subset.items())
    resources = [r for r in server.api.get_api_fn('GET', 'ec2', method)()
                 if is_subset(kw, r)]
    assert len(resources) != 0, "'%r' doesn't have resource '%s'" % (
        server, kw)


def assert_server_does_not_have_resource(server, method, resource_id):
    resources = server.api.get_api_fn('GET', 'ec2', method)(id=resource_id)
    assert len(resources) == 0, "'%r' has unexpected resource '%s'" % (
        server, resource_id)


def assert_post_forbidden(server, method, **kw):
    with pytest.raises(HttpError) as x_info:
        server.api.get_api_fn('POST', 'ec2', method)(**kw)
    assert x_info.value.status_code == 403


# https://networkoptix.atlassian.net/browse/VMS-2246
def test_create_and_remove_user_with_resource(one_running_mediaserver):
    user = generator.generate_user_data(
        user_id=1,  name="user1", email="user1@example.com",
        permissions="2432", cryptSha512Hash="", digest="",
        hash="", isAdmin=False, isEnabled=True, isLdap=False, realm="")
    one_running_mediaserver.api.ec2.saveUser.POST(**user)
    expected_permissions = "GlobalViewArchivePermission|GlobalManageBookmarksPermission|0x80"
    user_resource = [generator.generate_resource_params_data(id=1, resource=user)]
    one_running_mediaserver.api.ec2.setResourceParams.POST(json=user_resource)
    assert_server_has_resource(one_running_mediaserver, 'getUsers', id=user['id'], permissions=expected_permissions)
    assert_server_has_resource(
        one_running_mediaserver,
        'getResourceParams', resourceId=user['id'], name=user_resource[0]['name'])
    one_running_mediaserver.api.ec2.removeUser.POST(id=user['id'])
    assert_server_does_not_have_resource(one_running_mediaserver, 'getUsers', user['id'])
    assert_server_does_not_have_resource(one_running_mediaserver, 'getResourceParams', user['id'])
    assert not one_running_mediaserver.installation.list_core_dumps()


# https://networkoptix.atlassian.net/browse/VMS-3052
def test_missing_user_role(one_running_mediaserver):
    user_1 = generator.generate_user_data(user_id=1, name="user1", email="user1@example.com")
    user_2 = generator.generate_user_data(user_id=2, name="user2", email="user2@example.com",
                                          userRoleId=UNEXISTENT_USER_ROLE_GUIID)
    # Try link existing user to a missing role
    one_running_mediaserver.api.ec2.saveUser.POST(**user_1)
    assert_server_has_resource(one_running_mediaserver, 'getUsers', id=user_1['id'])
    user_1_with_unexpected_role = dict(user_1, userRoleId=UNEXISTENT_USER_ROLE_GUIID)
    assert_post_forbidden(one_running_mediaserver, 'saveUser', **user_1_with_unexpected_role)

    # Try create new user to non-existent role
    assert_post_forbidden(one_running_mediaserver, 'saveUser', **user_2)

    assert_server_has_resource(one_running_mediaserver, 'getUsers', id=user_1['id'], userRoleId=user_1['userRoleId'])
    assert_server_does_not_have_resource(one_running_mediaserver, 'getUsers', user_2['id'])

    assert not one_running_mediaserver.installation.list_core_dumps()


# https://networkoptix.atlassian.net/browse/VMS-2904
def test_remove_child_resources(one_running_mediaserver):
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
        one_running_mediaserver.api.get_api_fn('POST', 'ec2', post_method)(**data)
        resource_params = [generator.generate_resource_params_data(id=1, resource=data)]
        one_running_mediaserver.api.ec2.setResourceParams.POST(json=resource_params)
        assert_server_has_resource(one_running_mediaserver, get_method, id=data['id'])
        assert_server_has_resource(one_running_mediaserver, 'getResourceParams', resourceId=data['id'])
    # Remove camera_2
    one_running_mediaserver.api.ec2.removeResource.POST(id=camera_1['id'])
    assert_server_does_not_have_resource(one_running_mediaserver, 'getCameras', camera_1['id'])
    # Remove running_linux_server and check that all running_linux_server child resources have been removed
    one_running_mediaserver.api.ec2.removeResource.POST(id=server_data['id'])
    for _, get_method, data in tested_calls:
        assert_server_does_not_have_resource(one_running_mediaserver, get_method, data['id'])
        assert_server_does_not_have_resource(one_running_mediaserver, 'getResourceParams', data['id'])

    assert not one_running_mediaserver.installation.list_core_dumps()


# https://networkoptix.atlassian.net/browse/VMS-3068
def test_http_header_server(one_running_mediaserver):
    url = one_running_mediaserver.api.url('ec2/testConnection')
    valid_auth = HTTPDigestAuth(one_running_mediaserver.api.user, one_running_mediaserver.api.password)
    response = requests.get(url, auth=valid_auth, timeout=REST_API_TIMEOUT_SEC)
    _logger.debug('%r headers: %s', one_running_mediaserver, response.headers)
    assert response.status_code == 200
    assert 'Server' in response.headers.keys(), "HTTP header 'Server' is expected"
    invalid_auth = HTTPDigestAuth('invalid', 'invalid')
    response = requests.get(url, auth=invalid_auth, timeout=REST_API_TIMEOUT_SEC)
    _logger.debug('%r headers: %s', one_running_mediaserver, response.headers)
    assert response.status_code == 401
    assert 'WWW-Authenticate' in response.headers.keys(), "HTTP header 'WWW-Authenticate' is expected"
    assert 'Server' not in response.headers.keys(), "Unexpected HTTP header 'Server'"
    assert not one_running_mediaserver.installation.list_core_dumps()


# https://networkoptix.atlassian.net/browse/VMS-3069
def test_static_vulnerability(one_running_mediaserver):
    filepath = one_running_mediaserver.installation.dir / 'var' / 'web' / 'static' / 'test.file'
    filepath.parent.mkdir(parents=True, exist_ok=True)
    filepath.write_text("This is just a test file.")
    url = one_running_mediaserver.api.url('') + 'static/../../test.file'
    response = requests.get(url)
    assert response.status_code == 403
    assert not one_running_mediaserver.installation.list_core_dumps()


# https://networkoptix.atlassian.net/browse/VMS-7775
def test_auth_with_time_changed(one_vm, mediaserver_installers, ca, artifact_factory):
    with timeless_mediaserver(one_vm, mediaserver_installers, ca, artifact_factory) as timeless_server:
        url = timeless_server.api.url('ec2/getCurrentTime')

        timeless_server.os_access.set_time(datetime.now(pytz.utc))
        wait_for_true(
            lambda: get_time(timeless_server.api).is_close_to(datetime.now(pytz.utc)),
            "time on {} is close to now".format(timeless_server))

        shift = timedelta(days=3)

        response = requests.get(url, auth=HTTPDigestAuth(timeless_server.api.user, timeless_server.api.password))
        authorization_header_value = response.request.headers['Authorization']
        _logger.info(authorization_header_value)
        response = requests.get(url, headers={'Authorization': authorization_header_value})
        response.raise_for_status()

        timeless_server.os_access.set_time(datetime.now(pytz.utc) + shift)
        wait_for_true(
            lambda: get_time(timeless_server.api).is_close_to(datetime.now(pytz.utc) + shift),
            "time on {} is close to now + {}".format(timeless_server, shift))

        response = requests.get(url, headers={'Authorization': authorization_header_value})
        assert response.status_code != 401, "Cannot authenticate after time changed on server"
        response.raise_for_status()

        assert not timeless_server.installation.list_core_dumps()


def test_uptime_is_monotonic(one_vm, mediaserver_installers, ca, artifact_factory):
    with timeless_mediaserver(one_vm, mediaserver_installers, ca, artifact_factory) as timeless_server:
        timeless_server.os_access.set_time(datetime.now(pytz.utc))
        first_uptime = timeless_server.api.api.statistics.GET()['uptimeMs']
        if not isinstance(first_uptime, (int, float)):
            _logger.warning("Type of uptimeMs is %s but expected to be numeric.", type(first_uptime).__name__)
        new_time = timeless_server.os_access.set_time(datetime.now(pytz.utc) - timedelta(minutes=1))
        wait_for_true(
            lambda: get_time(timeless_server.api).is_close_to(new_time),
            "time on {} is close to {}".format(timeless_server, new_time))
        second_uptime = timeless_server.api.api.statistics.GET()['uptimeMs']
        if not isinstance(first_uptime, (int, float)):
            _logger.warning("Type of uptimeMs is %s but expected to be numeric.", type(second_uptime).__name__)
        assert float(first_uptime) < float(second_uptime)
        assert not timeless_server.installation.list_core_dumps()


def test_frequent_restarts(one_running_mediaserver):
    """Test for running_linux_server restart REST api and functional test wrapper for it."""
    # Loop is unfolded here so that we can see which exact line is failed.
    one_running_mediaserver.restart_via_api(timeout=timedelta(seconds=10))
    one_running_mediaserver.restart_via_api(timeout=timedelta(seconds=10))
    one_running_mediaserver.restart_via_api(timeout=timedelta(seconds=10))
    assert not one_running_mediaserver.installation.list_core_dumps()


@pytest.mark.xfail(reason="https://networkoptix.atlassian.net/browse/VMS-7808")
@pytest.mark.xfail(reason="https://networkoptix.atlassian.net/browse/VMS-7809")
@pytest.mark.parametrize('path', [
    '/ec2/getFullInfoExtraSuffix', '/api/pingExtraSuffix',  # VMS-7809: Matches by prefix and returns 200.
    '/api/nonExistent', '/ec2/nonExistent'])  # VMS-7809: Redirects with 301 but not returns 404.
def test_non_existent_api_endpoints(one_running_mediaserver, path):
    auth = HTTPDigestAuth(one_running_mediaserver.api.user, one_running_mediaserver.api.password)
    response = requests.get(one_running_mediaserver.api.url(path), auth=auth, allow_redirects=False)
    assert response.status_code == 404, "Expected 404 but got %r"
    assert not one_running_mediaserver.installation.list_core_dumps()


def test_https_verification(one_running_mediaserver, ca):
    url = one_running_mediaserver.api.url('/api/ping', secure=True)
    assert url.startswith('https://')
    with warnings.catch_warnings(record=True) as warning_list:
        response = requests.get(url, verify=str(ca.cert_path))
    assert response.status_code == 200
    for warning in warning_list:
        _logger.warning("Warning collected: %s.", warning)
        assert not isinstance(warning, urllib3.exceptions.InsecureRequestWarning)
