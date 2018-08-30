import json
import time
import timeit
import uuid
from contextlib import contextmanager
from datetime import datetime, timedelta

import pytz
import requests
import six
# noinspection PyPackageRequirements
from Crypto.Cipher import AES
from netaddr import EUI, IPAddress, IPNetwork

from framework import media_stream
from framework.http_api import HttpApi, HttpClient, HttpError
from framework.utils import RunningTime, bool_to_str, str_to_bool
from framework.waiting import wait_for_true
from .switched_logging import SwitchedLogger, with_logger

_logger = SwitchedLogger(__name__, 'mediaserver_api')

DEFAULT_API_USER = 'admin'
INITIAL_API_PASSWORD = 'admin'
DEFAULT_API_PASSWORD = 'qweasd123'
MAX_CONTENT_LEN_TO_LOG = 1000


class MediaserverApiRequestError(Exception):
    pass


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


class MergeError(Exception):
    def __init__(self, local, remote, message):
        super(MergeError, self).__init__('Request {} to merge with {}: {}'.format(local, remote, message))
        self.local = local
        self.remote = remote


class ExplicitMergeError(MergeError):
    def __init__(self, local, remote, error, error_string):
        super(ExplicitMergeError, self).__init__(local, remote, "{:d} {}".format(error, error_string))
        self.error = error
        self.error_string = error_string


class IncompatibleServersMerge(ExplicitMergeError):
    pass


class MergeUnauthorized(ExplicitMergeError):
    pass


class AlreadyMerged(MergeError):
    def __init__(self, local, remote, common_system_id):
        super(AlreadyMerged, self).__init__(local, remote, "already in one local system {}".format(common_system_id))
        self.system_id = common_system_id


class InappropriateRedirect(Exception):
    def __init__(self, server_name, url, location):
        message = 'Mediaserver {} redirected {} to {}'.format(server_name, url, location)
        super(InappropriateRedirect, self).__init__(self, message)


class GenericMediaserverApi(HttpApi):
    """HTTP API that knows conventions and quirks of Mediaserver regardless of endpoint."""

    @classmethod
    def new(cls, alias, hostname, port, username='admin', password=INITIAL_API_PASSWORD, ca_cert=None):
        return cls(alias, HttpClient(hostname, port, username, password, ca_cert=ca_cert))

    def __repr__(self):
        return '<GenericMediaserverApi at {}>'.format(self.http.url(''))

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
                raise Unauthorized(self.alias, response.request.url, response.status_code, reason, response_data)
            raise HttpError(self.alias, response.request.url, response.status_code, reason, response_data)
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
            raise MediaserverApiError(self.alias, response.request.url, error_code, response_data['errorString'])
        return response_data['reply']

    def request(self, method, path, secure=False, timeout=None, **kwargs):
        try:
            response = self.http.request(method, path, secure=secure, timeout=timeout, **kwargs)
        except (requests.Timeout, requests.ConnectionError) as e:
            raise MediaserverApiRequestError('%r: %s %r: %s' % (self, method, path, e))
        if response.is_redirect:
            raise InappropriateRedirect(self.alias, response.request.url, response.next.url)
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
        try:
            return other.start == self.start and other.duration == self.duration
        except AttributeError:
            return NotImplemented

    def __ne__(self, other):
        return not self == other


class MediaserverApi(object):
    """Collection of front-end methods to work with HTTP API with handy ins and outs."""

    def __init__(self, generic_api):  # type: (GenericMediaserverApi) -> None
        # `.generic` should be rarely used, only when no request and/or response processing is required.
        # Most existing usages of `.generic` should be transformed into methods hereof.
        self.generic = generic_api
        # TODO: Split this class into composing parts: `SystemApi`, `CamerasApi`, etc.

    def __str__(self):
        return self.generic.http.url('')

    def __repr__(self):
        return '<MediaserverApi at {}>'.format(self)

    def auth_key(self, method):
        path = ''
        response = self.generic.get('api/getNonce')
        realm, nonce = response['realm'], response['nonce']
        return self.generic.http.auth_key(method, path, realm, nonce)

    def is_online(self):
        try:
            self.generic.get('/api/ping')
        except MediaserverApiRequestError:
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

    _setup_logger = _logger.getChild('setup')

    @with_logger(_setup_logger, 'framework.waiting')
    @with_logger(_setup_logger, 'framework.http_api')
    @with_logger(_setup_logger, _logger.name)
    def setup_local_system(self, system_settings=None):
        system_settings = system_settings or {}
        _logger.info('Setup local system on %s.', self)
        response = self.generic.post('api/setupLocalSystem', {
            'password': DEFAULT_API_PASSWORD,
            'systemName': self.generic.alias,
            'systemSettings': system_settings,
            })
        assert system_settings == {key: response['settings'][key] for key in system_settings.keys()}
        self.generic.http.set_credentials(self.generic.http.user, DEFAULT_API_PASSWORD)
        wait_for_true(lambda: self.get_local_system_id() != uuid.UUID(int=0), "local system is set up")
        _logger.info('Setup local system: complete, local system id: %s', self.get_local_system_id())
        return response['settings']

    @with_logger(_setup_logger, 'framework.waiting')
    @with_logger(_setup_logger, 'framework.http_api')
    @with_logger(_setup_logger, _logger.name)
    def setup_cloud_system(self, cloud_account, system_settings=None):
        _logger.info('Setting up server as cloud system %s:', self)
        system_settings = system_settings or {}
        bind_info = cloud_account.bind_system(self.generic.alias)
        request = {
            'systemName': self.generic.alias,
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

    def toggle_system_setting(self, name):
        old = str_to_bool(self.get_system_settings()[name])
        new = not old
        self.set_system_settings({name: bool_to_str(new)})
        assert str_to_bool(self.get_system_settings()[name]) == new
        return old

    def get_local_system_id(self):
        return uuid.UUID(self.generic.get('api/ping')['localSystemId'])

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
        started_at = datetime.now(pytz.utc)
        time_response = self.generic.get('/api/gettime')
        received = datetime.fromtimestamp(float(time_response['utcTime']) / 1000., pytz.utc)
        return RunningTime(received, datetime.now(pytz.utc) - started_at)

    def is_primary_time_server(self):
        response = self.generic.get('api/systemSettings')
        return response['settings']['primaryTimeServer'] == self.get_server_id()

    @contextmanager
    def waiting_for_restart(self, timeout_sec):
        old_runtime_id = self.generic.get('api/moduleInformation')['runtimeId']
        _logger.info("Runtime id before restart: %s", old_runtime_id)
        started_at = timeit.default_timer()
        yield
        failed_connections = 0
        while True:
            try:
                response = self.generic.get('api/moduleInformation')
            except MediaserverApiRequestError as e:
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

    def restart_via_api(self, timeout_sec=10):
        with self.waiting_for_restart(timeout_sec):
            self.generic.get('api/restart')

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

    @contextmanager
    def camera_recording(self, camera_id, fps=15):
        if not camera_id:
            raise ValueError("Camera ID is empty, is it registered?")
        schedule_tasks = [
            {
                'afterThreshold': 5,
                'beforeThreshold': 5,
                'dayOfWeek': day_of_week,
                'endTime': 86400,
                'fps': fps,
                'recordAudio': False,
                'recordingType': "RT_Always",
                'startTime': 0,
                'streamQuality': "high",
                }
            for day_of_week in [1, 2, 3, 4, 5, 6, 7]]
        self.generic.post('ec2/saveCameraUserAttributes', {
            'cameraId': camera_id,
            'scheduleEnabled': True,
            'scheduleTasks': schedule_tasks,
            })
        try:
            yield
        finally:
            self.generic.post('ec2/saveCameraUserAttributes', {
                'cameraId': camera_id,
                'scheduleEnabled': False,
                })

    @contextmanager
    def camera_audio_enabled(self, camera_id):
        attributes = self.get_camera_user_attributes_list(camera_id)[0]
        attributes['audioEnabled'] = True
        self.set_camera_user_attributes(**attributes)
        try:
            yield
        finally:
            attributes['audioEnabled'] = False
            self.set_camera_user_attributes(**attributes)

    def rebuild_archive(self):
        self.generic.get('api/rebuildArchive', params=dict(mainPool=1, action='start'))
        for i in range(30):
            response = self.generic.get('api/rebuildArchive', params=dict(mainPool=1))
            if response['state'] == 'RebuildState_None':
                return
            time.sleep(0.3)
        assert False, 'Timed out waiting for archive to rebuild'

    def get_recorded_time_periods(self, camera_id):
        if not camera_id:
            raise ValueError("Camera ID is empty, is it registered?")
        periods = [
            TimePeriod(datetime.utcfromtimestamp(int(d['startTimeMs']) / 1000.).replace(tzinfo=pytz.utc),
                       timedelta(seconds=int(d['durationMs']) / 1000.))
            for d in self.generic.get('ec2/recordedTimePeriods', params=dict(cameraId=camera_id, flat=True))]
        _logger.info('Mediaserver %r returned %d recorded periods:', self.generic.alias, len(periods))
        for period in periods:
            _logger.info('\t%s', period)
        return periods

    def get_media_stream(self, stream_type, camera_mac_addr):
        assert stream_type in ['rtsp', 'webm', 'hls', 'direct-hls'], repr(stream_type)
        server_url = self.generic.http.url('')
        user = self.generic.http.user
        password = self.generic.http.password
        if stream_type == 'webm':
            return media_stream.Webm(server_url, user, password, camera_mac_addr)
        if stream_type == 'rtsp':
            return media_stream.Rtsp(server_url, user, password, camera_mac_addr)
        if stream_type == 'hls':
            return media_stream.M3uHls(server_url, user, password, camera_mac_addr)
        if stream_type == 'direct-hls':
            return media_stream.DirectHls(server_url, user, password, camera_mac_addr)
        assert False, 'Unknown stream type: %r; known are: rtsp, webm, hls and direct-hls' % stream_type

    def set_camera_advanced_param(self, camera_id, **params):  # types: (str, dict) -> None
        """Take a camera id as a string and a **params dict ({param_name1: param1_value, ...})
        and perform a GET request to the server to update camera's advanced parameters.
        """
        params.update({'cameraId': camera_id})
        # Although api/setCameraParam method is considered POST in doc, in the code it is GET
        self.generic.get('api/setCameraParam', params)

    def get_camera_user_attributes_list(self, camera_id):  # type: (str) -> list
        """Get user attributes for a specific camera"""
        return self.generic.get('ec2/getCameraUserAttributesList', params=dict(id=camera_id))

    def set_camera_user_attributes(self, camera_id='', **params):  # type: (str, dict) -> None
        """Sets the camera user attribute(-s) for a specific camera
           WARNING! camera_id format has to be UUID! MAC doesn't work for this method. If no
           camera_id is specified, it has to be already in params.
        """
        if len(camera_id) != 0:
            params['cameraId'] = camera_id
        assert 'cameraId' in params
        self.generic.post('ec2/saveCameraUserAttributes', params)

    @classmethod
    def _parse_json_fields(cls, data):
        if isinstance(data, dict):
            return {k: cls._parse_json_fields(v) for k, v in data.items()}
        if isinstance(data, list):
            return [cls._parse_json_fields(i) for i in data]
        if isinstance(data, six.string_types):
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

    def get_resource(self, path, id):
        resources = self.get_resources(path, params=dict(id=id))
        assert len(resources) == 1
        return resources[0]

    def remove_resource(self, id):
        self.generic.post('ec2/removeResource', dict(id=id))

    def set_camera_credentials(self, id, login, password):
        # Do not try to understand this code, this is hardcoded the same way as in common library.
        data = ':'.join([login, password])
        data += chr(0) * (16 - (len(data) % 16))
        key = '4453D6654C634636990B2E5AA69A1312'.decode('hex')
        iv = '000102030405060708090a0b0c0d0e0f'.decode('hex')
        c = AES.new(key, AES.MODE_CBC, iv).encrypt(data).encode('hex')
        self.generic.post("ec2/setResourceParams", [dict(resourceId=id, name='credentials', value=c)])

    def interfaces(self):
        response = self.generic.get('api/iflist')
        networks = [
            IPNetwork('{ipAddr}/{netMask}'.format(**interface))
            for interface in response]
        return networks

    _merge_logger = _logger.getChild('merge')

    @with_logger(_setup_logger, 'framework.waiting')
    @with_logger(_setup_logger, 'framework.http_api')
    @with_logger(_setup_logger, _logger.name)
    def merge(
            self,
            remote_api,  # type: MediaserverApi
            remote_address,  # type: IPAddress
            remote_port,
            take_remote_settings=False,
            ):
        # When many servers are merged, there is server visible from others.
        # This server is passed as remote. That's why it's higher in loggers hierarchy.
        logger = self._merge_logger.getChild(remote_api.generic.alias).getChild(self.generic.alias)
        logger.info("Merge %s to %s (takeRemoteSettings: %s):", self, remote_api, take_remote_settings)
        master_api, servant_api = (remote_api, self) if take_remote_settings else (self, remote_api)
        master_system_id = master_api.get_local_system_id()
        logger.debug("Settings from %r, system id %s.", master_api, master_system_id)
        servant_system_id = servant_api.get_local_system_id()
        logger.debug("Other system id %s.", servant_system_id)
        if servant_system_id == master_system_id:
            raise AlreadyMerged(self, remote_api, master_system_id)
        try:
            self.generic.post('api/mergeSystems', {
                'url': 'http://{}:{}/'.format(remote_address, remote_port),
                'getKey': remote_api.auth_key('GET'),
                'postKey': remote_api.auth_key('POST'),
                'takeRemoteSettings': take_remote_settings,
                'mergeOneServer': False,
                })
        except MediaserverApiError as e:
            if e.error_string == 'INCOMPATIBLE':
                raise IncompatibleServersMerge(self, remote_api, e.error, e.error_string)
            if e.error_string == 'UNAUTHORIZED':
                raise MergeUnauthorized(self, remote_api, e.error, e.error_string)
            raise ExplicitMergeError(self, remote_api, e.error, e.error_string)
        servant_api.generic.http.set_credentials(master_api.generic.http.user, master_api.generic.http.password)
        wait_for_true(servant_api.credentials_work, timeout_sec=30)
        wait_for_true(
            lambda: servant_api.get_local_system_id() == master_system_id,
            "{} responds with system id {}".format(servant_api, master_system_id),
            timeout_sec=10)
        logger.info("Merge %s to %s (takeRemoteSettings: %s): complete.", self, remote_api, take_remote_settings)

    def find_camera(self, camera_mac):
        for camera_info in self.generic.get('ec2/getCamerasEx'):
            if EUI(camera_info['physicalId']) == EUI(camera_mac):
                _logger.info("Camera %r is discovered by server %r as %r", camera_mac, self, camera_info['id'])
                return camera_info['id']

    def make_event_rule(
            self, event_type, event_state, action_type, event_resource_ids=[],
            event_condition_resource='', action_resource_ids=[]):
        self.generic.post('ec2/saveEventRule', dict(
            actionParams=json.dumps(dict(
                allUsers=False,
                authType='authBasicAndDigest',
                durationMs=5000,
                forced=True,
                fps=10,
                needConfirmation=False,
                playToClient=True,
                recordAfter=0,
                recordBeforeMs=1000,
                requestType='',
                streamQuality='highest',
                useSource=False,
            )),
            actionResourceIds=action_resource_ids,
            actionType=action_type,
            aggregationPeriod=0,
            comment='',
            disabled=False,
            eventCondition=json.dumps(dict(
                eventTimestampUsec='0',
                eventType='undefinedEvent',
                metadata=dict(allUsers=False),
                reasonCode='none',
                resourceName=event_condition_resource,
            )),
            eventResourceIds=event_resource_ids,
            eventState=event_state,
            eventType=event_type,
            id='{%s}' % uuid.uuid1(),
            schedule='',
            system=False,
        ))

    def create_event(self, **params):
        self.generic.get('api/createEvent', params)

    def get_events(self, camera_id=None, type_=None, from_='2000-01-01', to_='3000-01-01'):
        query = {'from': from_, 'to': to_, 'cameraId': camera_id, 'event_type': type_}
        return self.generic.get('api/getEvents', {k: v for k, v in query.items() if v})
