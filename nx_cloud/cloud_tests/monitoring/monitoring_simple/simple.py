# Specification is at https://networkoptix.atlassian.net/wiki/display/SD/Auto-test+for+cloud+portal+backend

import time
import sys
import requests
import logging
import boto3
import json
import docker

from urllib.parse import quote
from requests.auth import HTTPDigestAuth

logging.basicConfig(stream=sys.stderr, level=logging.INFO)
logging.getLogger("requests").setLevel(logging.WARNING)
logging.getLogger("botocore").setLevel(logging.WARNING)
logging.getLogger("boto3").setLevel(logging.WARNING)

log = logging.getLogger('simple_cloud_test')


def testclass(cls):
    test_methods_dict = {}

    for name, method in cls.__dict__.items():
        if hasattr(method, "testmethod_index"):
            test_methods_dict[method.testmethod_index] = method

    cls._test_methods = test_methods_dict.values()

    return cls


def testmethod(delay=0):
    def _testmethod(f):
        def wrapper(self):
            try:
                if delay:
                    log.info('Test {}: {}. Sleeping for {} seconds'.format(wrapper.testmethod_index, f.__name__, delay))
                    time.sleep(delay)

                log.info('Running test {}: {}'.format(wrapper.testmethod_index, f.__name__))
                f(self)
                log.info('Test {}: success'.format(f.__name__))
            except AssertionError as e:
                log.error('Test {}: failure. Message: '.format(f.__name__))
                log.error(e)
                raise
            except:
                log.error('Test {}: failure'.format(f.__name__))
                raise

        wrapper.testmethod_index = testmethod.counter
        testmethod.counter += 1

        return wrapper

    return _testmethod

testmethod.counter = 1


@testclass
class CloudSession(object):
    def __init__(self, host):
        """
            This class defines tests to be run.

            Test are being run in order they appear in the code.
        """
        self.host = host
        self.base_url = 'https://{}'.format(host)
        self.session = requests.Session()

        self.cloud_sender = 'Nx Cloud <service@networkoptix.com>'
        self.email = 'noptixqa-owner@hdw.mx'
        self.password = 'qweasd123'
        self.user_email = 'vasily@hdw.mx'

        self.vms_user_id = None
        self.system_id = None

    def _url(self, path):
        return '{}{}'.format(self.base_url, path)

    def wait_for_message(self, queue):
        sqs = boto3.resource('sqs')
        queue = sqs.get_queue_by_name(QueueName=queue)

        for i in range(3):
            for message in queue.receive_messages(WaitTimeSeconds=20):
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

    def run_tests(self):
        for method in self._test_methods:
            method(self)

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

    def assert_response_text_is_ok(self, response):
        response_text = '{"resultCode":"ok"}'
        self.assert_response_text_equals(response, response_text)

    def assert_response_text_equals(self, response, text):
        assert response.text == text, 'Response is invalid. Expected:\n{}\nActual:\n{}'.format(response.text, text)

    def assert_response_has_cookie(self, response, cookie):
        assert cookie in response.cookies, 'Response doesn\'t containt cookie {}'.format(cookie)

    @testmethod()
    def restore_password(self):
        r = self.post('/api/account/restorePassword', {"user_email": self.email})

        self.assert_response_text_is_ok(r)
        assert self.wait_for_message('noptixqa-owner-queue'), "Timeout waiting for e-mail to arrive"

    @testmethod()
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

        self.system_id = data[0]['id']

    @testmethod()
    def share_system(self):
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
        assert 'id' in data, 'No ID'

    @testmethod(delay=10)
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

    @testmethod()
    def remove_user(self):
        request_data = {'id': self.vms_user_id}
        auth = HTTPDigestAuth(self.email, self.password)
        self.post('/gateway/{system_id}/ec2/removeUser'.format(system_id=self.system_id),
                  request_data, auth=auth)

    @testmethod(delay=10)
    def check_vasily_is_absent(self):
        headers = {
            'referer': '{}/systems/{}'.format(self.base_url, self.system_id),
            'x-csrftoken': self.session.cookies['csrftoken']
        }
        r = self.get('/api/systems/{system_id}/users'.format(system_id=self.system_id), headers=headers)
        data = r.json()
        assert data[0]['systemId'] == self.system_id, 'Wrong System ID'

        assert next(filter(lambda x: x['accountEmail'] == self.user_email, data), None) is None, 'User still exists'

    @testmethod()
    def test_cloud_connect(self):
        command = '--http-client --url=http://{user}:{password}@{system_id}/ec2/getUsers'.format(
            user=quote(self.email),
            password=quote(self.password),
            system_id=self.system_id)

        client = docker.client.from_env()
        output = client.containers.run(
            '009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/cloud_connect_test_util:3.1.0.13200', command)
        assert b'HTTP/1.1 200 OK' in output, 'Received invalid output from cloud connect'


def main():
    host = sys.argv[1]

    session = CloudSession(host)
    session.run_tests()
    session.close()

if __name__ == '__main__':
    main()
