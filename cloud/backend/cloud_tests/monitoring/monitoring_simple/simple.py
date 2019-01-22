# Specification is at https://networkoptix.atlassian.net/wiki/display/SD/Auto-test+for+cloud+portal+backend
from contextlib import contextmanager
from io import StringIO
import time
import sys
import logging
import json
import traceback
from urllib.parse import quote
from datetime import datetime

import requests
import boto3
import docker
from requests.auth import HTTPDigestAuth
from requests.adapters import HTTPAdapter
from requests.packages.urllib3.util.retry import Retry

MEDIASERVER_VERSION = '3.1.0.17256'
CLOUD_CONNECT_TEST_UTIL_VERSION = '18.3.0.20101'
RETRY_TIMEOUT = 10  # seconds

log = logging.getLogger('simple_cloud_test')


def requests_retry_session(
        retries=3,
        backoff_factor=0.3,
        status_forcelist=(404, 500, 502, 504),
        session=None,
):
    session = session or requests.Session()
    retry = Retry(
        total=retries,
        read=retries,
        connect=retries,
        backoff_factor=backoff_factor,
        status_forcelist=status_forcelist,
    )
    adapter = HTTPAdapter(max_retries=retry)
    session.mount('http://', adapter)
    session.mount('https://', adapter)
    return session


def setup_logging():
    logging.basicConfig(stream=sys.stderr, level=logging.INFO)
    logging.getLogger("requests").setLevel(logging.DEBUG)
    logging.getLogger("botocore").setLevel(logging.WARNING)
    logging.getLogger("boto3").setLevel(logging.WARNING)


def testclass(cls):
    test_methods_dict = {}
    metric_keys = []

    for name, method in cls.__dict__.items():
        if hasattr(method, "testmethod_index"):
            test_methods_dict[method.testmethod_index] = method

        if hasattr(method, "metric") and method.metric:
            host = method.host if hasattr(method, "host") and method.host else None
            metric_keys.append((method.metric, host))

    cls._test_methods = test_methods_dict.values()
    cls._metric_keys = metric_keys

    return cls


def testmethod(delay=0, host=None, continue_if_fails=False, metric=None, tries=1, debug_skip=0):
    def _testmethod(f):
        def wrapper(self):
            try:
                if debug_skip and self.debug:
                    log.info("Skipping test {}: {}".format(wrapper.testmethod_index, f.__name__))
                    return

                if delay:
                    log.info('Test {}: {}. Sleeping for {} seconds'.format(wrapper.testmethod_index, f.__name__, delay))
                    time.sleep(delay)

                for n_try in range(1, tries + 1):
                    log.info(
                        'Running test {}: {}. Try {} of {}'.format(wrapper.testmethod_index, f.__name__, n_try, tries))
                    try:
                        f(self)
                        break
                    except AssertionError:
                        if n_try == tries:
                            raise

                        log.error('Test {}: failed. Assertion failed. Retrying...'.format(f.__name__))

                        io = StringIO()
                        traceback.print_exc(file=io)
                        log.error(io.getvalue())

                        time.sleep(RETRY_TIMEOUT)
                    except Exception as e:
                        if n_try == tries:
                            raise

                        log.error('Test {}: failed. Exception {} occured. Retrying...'.format(f.__name__, type(e).__name__))

                        io = StringIO()
                        traceback.print_exc(file=io)
                        log.error(io.getvalue())

                        time.sleep(RETRY_TIMEOUT)

                log.info('Test {}: success'.format(f.__name__))

                if metric:
                    self.collected_metrics[(metric, host, datetime.utcnow())] = 0

                return 0
            except:
                log.error('Test {}: failure'.format(f.__name__))

                io = StringIO()
                traceback.print_exc(file=io)
                log.error(io.getvalue())

                if metric:
                    self.collected_metrics[(metric, host, datetime.utcnow())] = 1

                if continue_if_fails:
                    return 1

                raise

        wrapper.testmethod_index = testmethod.counter
        wrapper.metric = metric
        wrapper.host = host
        testmethod.counter += 1

        return wrapper

    return _testmethod


testmethod.counter = 1


@testclass
class CloudSession(object):
    def __init__(self, host, debug):
        """
            This class defines tests to be run.

            Test are being run in order they appear in the code.
        """
        self.host = host
        self.debug = debug

        self.base_url = 'https://{}'.format(host)
        self.session = requests.Session()

        self.cloud_sender = 'Nx Cloud <service@networkoptix.com>'
        self.email = 'noptixqa-owner@hdw.mx'
        self.password = 'qweasd123'
        self.user_email = 'vasily@hdw.mx'

        self.auth = HTTPDigestAuth(self.email, self.password)

        self.vms_user_id = None
        self.system_id = None

        self.collected_metrics = {}

    def _url(self, path):
        return '{}{}'.format(self.base_url, path)

    def purge_queue(self, queue):
        log.info('Purging queue')
        queue.purge()

        log.info('Sleeping 60 seconds')
        time.sleep(60)

    def wait_for_message(self, queue, tries=3, seconds_per_try=20):
        for i in range(tries):
            for message in queue.receive_messages(WaitTimeSeconds=seconds_per_try):
                try:
                    body = json.loads(message.body)
                    msg = json.loads(body['Message'])
                    if self.cloud_sender in msg['mail']['commonHeaders']['from']:
                        return True
                except (ValueError, KeyError):
                    pass
                finally:
                    message.delete()

            log.info('Still waiting...')

        return False

    def close(self):
        self.session.close()

    def _metric_data(self):
        metric_data = []

        for (metric, host, timestamp), value in self.collected_metrics.items():
            if value is None:
                continue

            metric_data_item = {
                'MetricName': metric,
                'Timestamp': timestamp,
                'Value': value,
                'Unit': 'Count',
            }

            if host:
                metric_data_item['Dimensions'] = [
                    {
                        'Name': 'host',
                        'Value': host
                    }
                ]

            metric_data.append(metric_data_item)

        return metric_data

    def _dynamodb_item(self):
        metric_items = []

        for (metric, host, timestamp), value in self.collected_metrics.items():
            metric_item = {
                'name': metric,
                'timestamp': timestamp.isoformat(),
                'value': value
            }

            if host:
                metric_item['dimensions'] = {
                    'host': host
                }

            metric_items.append(metric_item)

        timestamp = datetime.utcnow()
        return {
            'year': timestamp.year,
            'timestamp': timestamp.isoformat(),
            'metrics': metric_items
        }

    def report_metrics(self):
        collected_keys = set([(metric, host) for (metric, host, timestamp) in self.collected_metrics.keys()])

        timestamp = datetime.utcnow()

        for metric, host in self._metric_keys:
            if (metric, host) not in collected_keys:
                self.collected_metrics[(metric, host, timestamp)] = None

        cloudwatch = boto3.client('cloudwatch')
        cloudwatch.put_metric_data(
            Namespace='prod__monitoring',
            MetricData=self._metric_data()
        )

        dynamodb = boto3.resource('dynamodb')
        table = dynamodb.Table('prod-health-records')
        table.put_item(Item=self._dynamodb_item())

    def run_tests(self):
        status = 0

        try:
            for method in self._test_methods:
                status = method(self) or status
        except Exception as e:
            status = 1
        finally:
            if not self.debug:
                self.report_metrics()

        return status

    def get(self, path, headers={}, **kwargs):
        url = self._url(path)

        log.debug('Executing GET request:\nURL: {}\n'.format(url))

        response = self.session.get(url, headers=headers, **kwargs)
        assert response.status_code == 200, 'ERROR: Status code is {}'.format(response.status_code)

        return response

    def post(self, path, json_data, headers={}, **kwargs):
        url = self._url(path)

        log.debug('Executing POST request:\n URL: {}\n Data:\n{}\n'.format(url, json.dumps(json_data, indent=4)))

        r = self.session.post(url, json=json_data, headers=headers, **kwargs)

        assert r.status_code == 200, 'ERROR:\n Status code is {}\n Body:{}\n'.format(r.status_code, r.text)

        return r

    @staticmethod
    def assert_response_text_is_ok(response):
        response_text = '{"resultCode":"ok"}'
        CloudSession.assert_response_text_equals(response, response_text)

    @staticmethod
    def assert_response_text_equals(response, text):
        assert response.text == text, 'Response is invalid. Expected:\n{}\nActual:\n{}'.format(response.text, text)

    @staticmethod
    def assert_response_has_cookie(response, cookie):
        assert cookie in response.cookies, 'Response doesn\'t containt cookie {}'.format(cookie)

    @testmethod()
    def test_cloud_db_get_system_id(self):
        r = requests.get(self._url('/cdb/system/get'), auth=self.auth).json()

        assert 'systems' in r, 'Invalid response from cloud_db. No systems'
        assert len(r['systems']) == 1, 'Invalid response from cloud_db. Number of systems != 1'
        assert r['systems'][0]['id'], 'System ID is invalid'

        self.system_id = r['systems'][0]['id']

        log.info('Got system ID: {}'.format(self.system_id))

    def test_cctu_base(self, command):
        image = '009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/cloud_connect_test_util:{}'.format(
            CLOUD_CONNECT_TEST_UTIL_VERSION)

        log.info('Running image: {} command: {}'.format(image, command))

        client = docker.client.from_env()
        container = client.containers.run(image, command, detach=True)
        status = container.wait()
        log.info('Container exited with exit status {}'.format(status))
        out = container.logs(stdout=True, stderr=True)

        log.info('Output:\n{}'.format(out.decode('utf-8')))
        container.remove()

        assert b'HTTP/1.1 200 OK' in out, 'Received invalid output from cloud connect (command: {})'.format(
            command)
        assert status['StatusCode'] == 0, 'Cloud connect test util exited with non-zero status {}'.format(status)

    def test_cloud_connect_base(self, extra_args=''):
        command = '--log-level=DEBUG2 --http-client --url=http://{user}:{password}@{system_id}/ec2/getUsers {extra_args}'.format(
            user=quote(self.email),
            password=quote(self.password),
            system_id=self.system_id,
            extra_args=extra_args)

        self.test_cctu_base(command)

    @testmethod(metric='p2p_connections_failure', continue_if_fails=True)
    def test_cloud_connect_no_proxy(self):
        self.test_cloud_connect_base('--cloud-connect-disable-proxy')

    @testmethod(metric='proxy_connections_failure', continue_if_fails=True)
    def test_cloud_connect_proxy(self):
        self.test_cloud_connect_base('--cloud-connect-enable-proxy-only')

    @testmethod(metric='cloud_authentication_failure', continue_if_fails=True)
    def test_mediaserver_direct(self):
        r = requests.get('https://{}.relay.vmsproxy.com/web/api/moduleInformation'.format(self.system_id))
        mi = r.json()

        mediaserver_ip = None
        for ip in mi['reply']['remoteAddresses']:
            # Here we assume the test and mediaserver are in the same docker network
            if ip.startswith("172."):
                mediaserver_ip = ip
                break

        if not mediaserver_ip:
            log.warning("Can't find mediaserver IP")
            return

        ms_url = 'http://{}:7001/ec2/getUsers'.format(mediaserver_ip)
        r = requests.get(ms_url, auth=self.auth)

        assert r.status_code == 200, 'ERROR: Status code is {}'.format(r.status_code)

    def test_traffic_relay(self, relay_name):
        command = '--log-level=DEBUG2 --test-relay --relay-url=http://{}.vmsproxy.com/'.format(relay_name)
        self.test_cctu_base(command)

    @testmethod(metric='traffic_relay_failure', host='relay-la', continue_if_fails=True)
    def test_traffic_relay_la(self):
        self.test_traffic_relay('relay-la')

    @testmethod(metric='traffic_relay_failure', host='relay-ny', continue_if_fails=True)
    def test_traffic_relay_ny(self):
        self.test_traffic_relay('relay-ny')

    @testmethod(metric='traffic_relay_failure', host='relay-fr', continue_if_fails=True)
    def test_traffic_relay_fr(self):
        self.test_traffic_relay('relay-fr')

    @testmethod(metric='traffic_relay_failure', host='relay-sy', continue_if_fails=True)
    def test_traffic_relay_sy(self):
        self.test_traffic_relay('relay-sy')

    @testmethod(metric='email_failure', continue_if_fails=True, debug_skip=True)
    def restore_password(self):
        sqs = boto3.resource('sqs')
        queue = sqs.get_queue_by_name(QueueName='noptixqa-owner-queue')

        self.purge_queue(queue)

        r = self.post('/api/account/restorePassword', {"user_email": self.email})

        self.assert_response_text_is_ok(r)

        assert self.wait_for_message(queue), "Timeout waiting for e-mail to arrive"

    # We stop here. No other metrics would be reported if we couldn't log in to the system
    @testmethod(metric='cloud_portal_failure')
    def login(self):
        r = self.post('/api/account/login', {'email': self.email, 'password': self.password, 'remember': True})

        self.assert_response_has_cookie(r, 'csrftoken')
        self.assert_response_has_cookie(r, 'sessionid')

    @testmethod()
    def list_systems(self):
        r = self.get('/api/systems')
        data = r.json()
        assert len(data) > 0, 'Can\'t find the system'
        assert data[0]['id'], 'Got system without ID'

        assert self.system_id == data[0]['id'], 'Invalid response from portal. Invalid system id'

    @testmethod(tries=3, debug_skip=True)
    def share_system(self):
        request_data = {
            "systemId": self.system_id,
            "accountEmail": self.user_email,
            "accessRole": "liveViewer"
        }

        r = requests.post('{}/cdb/system/share'.format(self.base_url),
                          json=request_data, auth=self.auth)

        assert r.status_code == 200, "Failed to share via cdb. Code: {}".format(r.status_code)
        data = r.json()
        log.info('Share response: {}'.format(data))

    def share_system_via_vms(self):
        request_data = {
            "email": self.user_email,
            "name": self.user_email,
            "userRoleId": "{00000000-0000-0000-0000-000000000000}",
            "permissions": "GlobalAccessAllMediaPermission|GlobalAdminPermission|GlobalControlVideoWallPermission|GlobalEditCamerasPermission|GlobalExportPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalViewArchivePermission|GlobalViewBookmarksPermission|GlobalViewLogsPermission",
            "isCloud": True,
            "isEnabled": True
        }

        headers = {
            'referer': '{}/systems/{}'.format(self.base_url, self.system_id),
            'x-csrftoken': self.session.cookies['csrftoken']
        }
        r = self.post('/api/systems/{system_id}/proxy/ec2/saveUser'.format(system_id=self.system_id),
                      request_data, headers=headers)

        data = r.json()
        log.info('Share response: {}'.format(data))

        assert 'id' in data, 'No ID'

    @testmethod(delay=30, metric='view_and_settings_failure', tries=3, debug_skip=True)
    def check_system_users(self):
        headers = {
            'referer': '{}/systems/{}'.format(self.base_url, self.system_id),
            'x-csrftoken': self.session.cookies['csrftoken']
        }
        r = self.get('/api/systems/{system_id}/users'.format(system_id=self.system_id), headers=headers)
        data = r.json()
        assert data[0]['systemId'] == self.system_id, 'Wrong System ID'

        user = next(filter(lambda x: x['accountEmail'] == self.user_email, data), None)
        assert user is not None, 'No users returned'
        self.vms_user_id = user['vmsUserId']

        r = requests.get('https://{system_id}.relay.vmsproxy.com/ec2/getUsers'.format(system_id=self.system_id),
                      auth=self.auth)
        assert r.status_code == 200, "Failed to get users from vms. Code: {}".format(r.status_code)

        data = r.json()
        user = next(filter(lambda x: x['email'] == self.user_email, data), None)
        assert user is not None, 'No users returned'

    @testmethod(debug_skip=True)
    def remove_user(self):
        request_data = {
            "systemId": self.system_id,
            "accountEmail": self.user_email,
            "accessRole": "none"
        }

        r = requests.post('{}/cdb/system/share'.format(self.base_url),
                          json=request_data, auth=self.auth)

        assert r.status_code == 200, "Failed to remove user via cdb. Code: {}".format(r.status_code)
        data = r.json()
        log.info('Remove response: {}'.format(data))

    def remove_user_via_vms(self):
        request_data = {'id': self.vms_user_id}
        requests.post('https://{system_id}.relay.vmsproxy.com/ec2/removeUser'.format(system_id=self.system_id),
                      json=request_data, auth=self.auth)

    @testmethod(delay=20, debug_skip=True, tries=3)
    def check_vasily_is_absent(self):
        headers = {
            'referer': '{}/systems/{}'.format(self.base_url, self.system_id),
            'x-csrftoken': self.session.cookies['csrftoken']
        }
        # get users from cloud portal
        r = self.get('/api/systems/{system_id}/users'.format(system_id=self.system_id), headers=headers)
        data = r.json()
        assert data[0]['systemId'] == self.system_id, 'Wrong System ID'

        assert next(filter(lambda x: x['accountEmail'] == self.user_email, data),
                    None) is None, 'User still exists in portal'

        # get users from mediaserver
        r = requests.get('https://{system_id}.relay.vmsproxy.com/ec2/getUsers'.format(system_id=self.system_id),
                      auth=self.auth)
        assert r.status_code == 200, "Failed to get users from vms. Code: {}".format(r.status_code)

        data = r.json()
        assert next(filter(lambda x: x['email'] == self.user_email, data),
                    None) is None, 'User still exists in vms'


@contextmanager
def mediaserver():
    image = "009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/mediaserver:{}".format(MEDIASERVER_VERSION)

    client = docker.client.from_env()

    volumes = {'/srv/containers/mediaserver/etc': {'bind': '/opt/networkoptix/mediaserver/etc', 'mode': 'rw'},
                '/srv/containers/mediaserver/var': {'bind': '/opt/networkoptix/mediaserver/var', 'mode': 'rw'}}

    log.info('Running mediaserver')
    container = client.containers.run(image, ports={7001: 7001},volumes=volumes,  detach=True)

    try:
        mediaserver_ip = client.containers.get(container.id).attrs['NetworkSettings']['IPAddress']

        for i in range(12):
            try:
                log.info('Testing mediaserver connection...')
                r = requests_retry_session().get('http://{}:7001/api/moduleInformation'.format(mediaserver_ip))
                assert r.status_code == 200, 'Status code != 200: {}'.format(r.status_code)
                response_data = r.json()
                system_id = response_data['reply']['cloudSystemId']
                if system_id:
                    break

                time.sleep(RETRY_TIMEOUT)
            except ConnectionError as e:
                assert False, "Couldn't connect to local mediaserver directly: {}".format(e)

        assert system_id, 'Couldn\'t get System ID'
        log.info('System ID: {}'.format(system_id))

        try:
            log.info('Waiting for connection through proxy...')

            r = requests_retry_session().get(
                'https://{}.relay.vmsproxy.com/web/api/moduleInformation'.format(system_id))
            assert r.status_code == 200, "Status != 200: {}".format(r.status_code)
            assert system_id == r.json()['reply']['cloudSystemId'], "Wrong system id"
        except ConnectionError as e:
            assert False, "Couldn't connect to local mediaserver through proxy: {}".format(e)

        yield
    finally:
        log.info('Stopping mediaserver')
        status = container.stop()
        log.info('Mediaserver exited with exit status {}'.format(status))
        # stdout = container.logs(stdout=True, stderr=False)
        # stderr = container.logs(stdout=False, stderr=True)
        #
        # log.info('Stdout:\n{}'.format(stdout.decode('utf-8')))
        # log.info('Stderr:\n{}'.format(stderr.decode('utf-8')))
        container.remove()


def main():
    setup_logging()

    host = sys.argv[1]

    debug = False

    if len(sys.argv) == 3:
        debug = sys.argv[2] == "debug"

    with mediaserver():
        session = CloudSession(host, debug)
        status = session.run_tests()
        session.close()

    return status


if __name__ == '__main__':
    sys.exit(main())
