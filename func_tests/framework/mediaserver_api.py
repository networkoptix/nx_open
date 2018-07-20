import json
import logging
import time
import timeit
from Crypto.Cipher import AES
from datetime import datetime, timedelta
from uuid import UUID

import pytz
import requests
from pytz import utc

from framework.camera import Camera, make_schedule_task
from framework.http_api import HttpApi, HttpClient, HttpError
from framework.media_stream import DirectHlsMediaStream, M3uHlsMediaStream, RtspMediaStream, WebmMediaStream
from framework.utils import RunningTime
from framework.waiting import wait_for_true

DEFAULT_API_USER = 'admin'
INITIAL_API_PASSWORD = 'admin'
DEFAULT_API_PASSWORD = 'qweasd123'
MAX_CONTENT_LEN_TO_LOG = 1000

_logger = logging.getLogger(__name__)


class Unauthorized(HttpError):
    pass


class MediaserverApiError(Exception):
    """Error received from server."""

    def __init__(self, server_name, url, error, error_string):
        super(MediaserverApiError, self).__init__(
            self,
            'Mediaserver {} at {} REST API request returned error: {} {}'.format(
                server_name, url, error, error_string))
        self.error = error
        self.error_string = error_string


class InappropriateRedirect(Exception):
    def __init__(self, server_name, url, location):
        message = 'Mediaserver {} redirected {} to {}'.format(server_name, url, location)
        super(InappropriateRedirect, self).__init__(self, message)


class GenericMediaserverApi(HttpApi):
    @classmethod
    def new(cls, alias, hostname, port, username='admin', password=INITIAL_API_PASSWORD, ca_cert=None):
        return cls(alias, HttpClient(hostname, port, username, password, ca_cert=ca_cert))

    def _raise_for_status(self, response):
        if 400 <= response.status_code < 600:
            reason = response.reason
            response_data = None
            if response.content:
                try:
                    response_data = response.json()
                except ValueError:
                    pass
                else:
                    error_string = response_data.get('errorString')
                    if error_string:
                        reason = error_string
            if response.status_code == 401:
                raise Unauthorized(self._alias, response.request.url, response.status_code, reason, response_data)
            raise HttpError(self._alias, response.request.url, response.status_code, reason, response_data)
        else:
            response.raise_for_status()

    def _retrieve_data(self, response):
        if not response.content:
            _logger.warning("Empty response.")
            return None
        try:
            response_data = response.json()
        except ValueError:
            _logger.warning("Non-JSON response:\n%s", response.content)
            return response.content
        _logger.debug("JSON response:\n%s", json.dumps(response_data, indent=4))
        if not isinstance(response_data, dict):
            return response_data
        try:
            error_code = int(response_data['error'])
        except KeyError:
            return response_data
        if error_code != 0:
            raise MediaserverApiError(self._alias, response.request.url, error_code, response_data['errorString'])
        return response_data['reply']

    def request(self, method, path, secure=False, timeout=None, **kwargs):
        response = self.http.request(method, path, secure=secure, timeout=timeout, **kwargs)
        if response.is_redirect:
            raise InappropriateRedirect(self._alias, response.request.url, response.next.url)
        data = self._retrieve_data(response)
        self._raise_for_status(response)
        return data


class TimePeriod(object):
    def __init__(self, start, duration):
        assert isinstance(start, datetime), repr(start)
        assert isinstance(duration, timedelta), repr(duration)
        self.start = start
        self.duration = duration

    def __repr__(self):
        return 'TimePeriod(%s, %s)' % (self.start, self.duration)

    def __eq__(self, other):
        return (isinstance(other, TimePeriod)
                and other.start == self.start
                and other.duration == self.duration)


class MediaserverApi(object):
    def __init__(self, generic_api):  # type: (GenericMediaserverApi) -> None
        self.generic = generic_api

    def auth_key(self, method):
        path = ''
        response = self.generic.get('api/getNonce')
        realm, nonce = response['realm'], response['nonce']
        return self.generic.http.auth_key(method, path, realm, nonce)

    def is_online(self):
        try:
            self.generic.get('/api/ping')
        except requests.RequestException:
            return False
        else:
            return True

    def credentials_work(self):
        try:
            self.generic.get('ec2/testConnection')
        except Unauthorized:
            return False
        return True

    def get_server_id(self):
        return self.generic.get('/ec2/testConnection')['ecsGuid']

    def setup_local_system(self, system_settings=None):
        system_settings = system_settings or {}
        _logger.info('Setup local system on %s.', self)
        response = self.generic.post('api/setupLocalSystem', {
            'password': DEFAULT_API_PASSWORD,
            'systemName': self.generic._alias,
            'systemSettings': system_settings,
            })
        assert system_settings == {key: response['settings'][key] for key in system_settings.keys()}
        self.generic.http.set_credentials(self.generic.http.user, DEFAULT_API_PASSWORD)
        wait_for_true(lambda: self.get_local_system_id() != UUID(int=0), "local system is set up")
        return response['settings']

    def setup_cloud_system(self, cloud_account, system_settings=None):
        _logger.info('Setting up server as cloud system %s:', self)
        system_settings = system_settings or {}
        bind_info = cloud_account.bind_system(self.generic._alias)
        request = {
            'systemName': self.generic._alias,
            'cloudAuthKey': bind_info.auth_key,
            'cloudSystemID': bind_info.system_id,
            'cloudAccountName': cloud_account.api.http.user,
            'systemSettings': system_settings,
            }
        response = self.generic.post('api/setupCloudSystem', request, timeout=300)
        assert system_settings == {key: response['settings'][key] for key in system_settings.keys()}
        # assert cloud_account.api.http.user == response['settings']['cloudAccountName']
        self.generic.http.set_credentials(cloud_account.api.http.user, cloud_account.password)
        assert self.credentials_work()
        return response['settings']

    def detach_from_cloud(self, password):
        self.generic.post('api/detachFromCloud', {'password': password})
        self.generic.http.set_credentials(DEFAULT_API_USER, password)

    def get_system_settings(self):
        settings = self.generic.get('/api/systemSettings')['settings']
        return settings

    def set_system_settings(self, new_settings):
        return self.generic.get('api/systemSettings', params=new_settings)

    def get_local_system_id(self):
        response = requests.get(self.generic.http.url('api/ping'))
        return UUID(response.json()['reply']['localSystemId'])

    def set_local_system_id(self, new_id):
        self.generic.get('/api/configure', params={'localSystemId': str(new_id)})

    def get_cloud_system_id(self):
        return self.get_system_settings()['cloudSystemID']

    def servers_is_online(self):
        servers_list = self.generic.get('ec2/getMediaServersEx')
        s_guid = self.get_server_id()
        this_servers = [v for v in servers_list if v['id'] == s_guid and v['status'] == 'Online']
        other_servers = [v for v in servers_list if v['id'] != s_guid and v['status'] == 'Online']
        return len(this_servers) == 1 and len(other_servers) != 0

    def neighbor_is_offline(self):
        servers_list = self.generic.get('ec2/getMediaServersEx')
        s_guid = self.get_server_id()
        this_servers = [v for v in servers_list if v['id'] == s_guid and v['status'] == 'Online']
        other_servers = [v for v in servers_list if v['id'] != s_guid]
        other_offline_servers = [v for v in other_servers if v['status'] == 'Offline']
        return len(this_servers) == 1 and other_servers == other_offline_servers

    def get_time(self):
        started_at = datetime.now(utc)
        time_response = self.generic.get('/api/gettime')
        received = datetime.fromtimestamp(float(time_response['utcTime']) / 1000., utc)
        return RunningTime(received, datetime.now(utc) - started_at)

    def is_primary_time_server(self):
        response = self.generic.get('api/systemSettings')
        return response['settings']['primaryTimeServer'] == self.get_server_id()

    def restart_via_api(self, timeout_sec=10):
        old_runtime_id = self.generic.get('api/moduleInformation')['runtimeId']
        _logger.info("Runtime id before restart: %s", old_runtime_id)
        started_at = timeit.default_timer()
        self.generic.get('api/restart')
        failed_connections = 0
        while True:
            try:
                response = self.generic.get('api/moduleInformation')
            except requests.ConnectionError as e:
                if timeit.default_timer() - started_at > timeout_sec:
                    assert False, "Mediaserver hasn't started, caught %r, timed out." % e
                _logger.debug("Expected failed connection: %r", e)
                failed_connections += 1
                time.sleep(timeout_sec)
                continue
            new_runtime_id = response['runtimeId']
            if new_runtime_id == old_runtime_id:
                if failed_connections > 0:
                    assert False, "Runtime id remains same after failed connections."
                if timeit.default_timer() - started_at > timeout_sec:
                    assert False, "Mediaserver hasn't stopped, timed out."
                _logger.warning("Mediaserver hasn't stopped yet, delay is acceptable.")
                time.sleep(timeout_sec)
                continue
            _logger.info("Mediaserver restarted successfully, new runtime id is %s", new_runtime_id)
            break

    def factory_reset(self):
        old_runtime_id = self.generic.get('api/moduleInformation')['runtimeId']
        self.generic.post('api/restoreState', {})

        def _mediaserver_has_restarted():
            try:
                current_runtime_id = self.generic.get('api/moduleInformation')['runtimeId']
            except requests.RequestException:
                return False
            return current_runtime_id != old_runtime_id

        wait_for_true(_mediaserver_has_restarted)

    def get_updates_state(self):
        response = self.generic.get('api/updates2/status')
        status = response['state']
        return status

    def add_camera(self, camera):
        assert not camera.id, 'Already added to a server with id %r' % camera.id
        params = camera.get_info(parent_id=self.get_server_id())
        result = self.generic.post('ec2/saveCamera', dict(**params))
        camera.id = result['id']
        return camera.id

    def _set_camera_recording(self, camera, recording, options=None):
        assert camera, 'Camera %r is not yet registered on server' % camera
        schedule_tasks = [make_schedule_task(day_of_week + 1) for day_of_week in range(7)]
        if options is not None:
            for task in schedule_tasks:
                task.update(options)
        self.generic.post('ec2/saveCameraUserAttributes', dict(
            cameraId=camera.id, scheduleEnabled=recording, scheduleTasks=schedule_tasks))

    def start_recording_camera(self, camera, options=None):
        self._set_camera_recording(camera, recording=True, options=options)

    def stop_recording_camera(self, camera, options=None):
        self._set_camera_recording(camera, recording=False, options=options)

    def rebuild_archive(self):
        self.generic.get('api/rebuildArchive', params=dict(mainPool=1, action='start'))
        for i in range(30):
            response = self.generic.get('api/rebuildArchive', params=dict(mainPool=1))
            if response['state'] == 'RebuildState_None':
                return
            time.sleep(0.3)
        assert False, 'Timed out waiting for archive to rebuild'

    def get_recorded_time_periods(self, camera):
        assert camera.id, 'Camera %r is not yet registered on server' % camera.name
        periods = [
            TimePeriod(datetime.utcfromtimestamp(int(d['startTimeMs']) / 1000.).replace(tzinfo=pytz.utc),
                       timedelta(seconds=int(d['durationMs']) / 1000.))
            for d in self.generic.get('ec2/recordedTimePeriods', params=dict(cameraId=camera.id, flat=True))]
        _logger.info('Mediaserver %r returned %d recorded periods:', self.generic._alias, len(periods))
        for period in periods:
            _logger.info('\t%s', period)
        return periods

    def get_media_stream(self, stream_type, camera):
        assert stream_type in ['rtsp', 'webm', 'hls', 'direct-hls'], repr(stream_type)
        assert isinstance(camera, Camera), repr(camera)
        server_url = self.generic.http.url('')
        user = self.generic.http.user
        password = self.generic.http.password
        camera_mac_addr = camera.mac_addr
        if stream_type == 'webm':
            return WebmMediaStream(server_url, user, password, camera_mac_addr)
        if stream_type == 'rtsp':
            return RtspMediaStream(server_url, user, password, camera_mac_addr)
        if stream_type == 'hls':
            return M3uHlsMediaStream(server_url, user, password, camera_mac_addr)
        if stream_type == 'direct-hls':
            return DirectHlsMediaStream(server_url, user, password, camera_mac_addr)
        assert False, 'Unknown stream type: %r; known are: rtsp, webm, hls and direct-hls' % stream_type

    @classmethod
    def _parse_json_fields(cls, data):
        if isinstance(data, dict):
            return {k: cls._parse_json_fields(v) for k, v in data.iteritems()}
        if isinstance(data, list):
            return [cls._parse_json_fields(i) for i in data]
        if isinstance(data, basestring):
            try:
                json_data = json.loads(data)
            except ValueError:
                pass
            else:
                return cls._parse_json_fields(json_data)
        return data

    def get_resources(self, path, *args, **kwargs):
        resources = self.generic.get('ec2/get' + path, *args, **kwargs)
        for resource in resources:
            for p in resource.pop('addParams', []):
                resource[p['name']] = p['value']
        return self._parse_json_fields(resources)

    def get_resource(self, path, id, **kwargs):
        resources = self.get_resources(path, params=dict(id=id))
        assert len(resources) == 1
        return resources[0]

    def set_camera_credentials(self, id, login, password):
        # Do not try to understand this code, this is hardcoded the same way as in common library.
        data = ':'.join([login, password])
        data += chr(0) * (16 - (len(data) % 16))
        key = '4453D6654C634636990B2E5AA69A1312'.decode('hex')
        iv = '000102030405060708090a0b0c0d0e0f'.decode('hex')
        c = AES.new(key, AES.MODE_CBC, iv).encrypt(data).encode('hex')
        self.generic.post("ec2/setResourceParams", [dict(resourceId=id, name='credentials', value=c)])
