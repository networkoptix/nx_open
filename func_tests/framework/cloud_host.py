import datetime
import logging
import json

import requests

from framework.http_api import HttpApi, HttpClient, HttpError
from framework.imap import IMAPConnection

_logger = logging.getLogger(__name__)


CLOUD_HOST_REGISTRY_URL = 'https://ireg.hdw.mx/api/v1/cloudhostfinder/'

IMAP_HOST = 'imap.gmail.com'
AUTOTEST_EMAIL_ALIAS = 'autotest@networkoptix.com'
AUTOTEST_LOGIN_EMAIL = 'service@networkoptix.com'
CLOUD_ACCOUNT_PASSWORD = 'qweasd123'

# timeout when waiting for activation email to appear in IMAP inbox
FETCH_ACTIVATION_EMAIL_TIMEOUT = datetime.timedelta(minutes=1)


class ServerBindInfo(object):

    def __init__(self, auth_key, system_id, status):
        self.auth_key = auth_key
        self.system_id = system_id
        self.status = status


class GenericCloudApi(HttpApi):
    def request(self, method, path, secure=False, timeout=None, **kwargs):
        response = self.http.request(method, path, secure=secure, timeout=timeout, **kwargs)
        response.raise_for_status()
        data = response.json()
        _logger.debug("JSON response:\n%s", json.dumps(data, indent=4))
        return data


# TODO: Split into `CloudApi` and `CloudAccount`.
class CloudAccount(object):

    def __init__(self, name, customization, hostname, user, password):
        self.name = name
        self.customization = customization
        self.hostname = hostname
        self.api = GenericCloudApi('cloud-host:%s' % name, HttpClient(self.hostname, 80, user, password))

    def __repr__(self):
        return '<CloudAccount {self.name} at {self.hostname}>'.format(self=self)

    @property
    def password(self):
        return self.api.http.password

    @property
    def url(self):
        return 'http://%s/' % self.hostname

    def ping(self):
        unused_realm = self.api.get('cdb/ping')

    def get_user_info(self):
        return self.api.get('cdb/account/get')

    def register_user(self, first_name, last_name):
        response = self.api.post('api/account/register', dict(
            email=self.api.http.user,
            password=self.api.http.password,
            first_name=first_name,
            last_name=last_name,
            subscribe=False,
            ))
        assert response == dict(resultCode='ok'), repr(response)

    def resend_activation_code(self):
        response = self.api.post('cdb/account/reactivate', dict(email=self.api.http.user))
        assert response == dict(code=''), repr(response)

    def activate_user(self, activation_code):
        response = self.api.post('cdb/account/activate', dict(code=activation_code))
        assert response.get('email') == self.api.http.user, repr(response)  # Got activation code for another user?

    def set_user_customization(self, customization):
        response = self.api.post('cdb/account/update', dict(customization=customization))
        assert response.get('resultCode') == 'ok'

    def check_user_is_valid(self):
        user_data = self.get_user_info()
        assert user_data.get('statusCode') == 'activated'

    def check_is_ready_for_tests(self):
        _logger.debug('Checking cloud host %r...', self)
        self.ping()
        _logger.debug('Cloud host %r is up', self)
        self.check_user_is_valid()
        _logger.info('Cloud host %r is up and test user is valid', self)

    def bind_system(self, system_name):
        response = self.api.get('cdb/system/bind', dict(
            name=system_name,
            customization=self.customization,
            ))
        return ServerBindInfo(response['authKey'], response['id'], response['status'])


class CloudEmail(object):

    def __init__(self, cloud_group, customization, account_idx):
        user_name, domain = AUTOTEST_EMAIL_ALIAS.split('@')
        params = dict(
            user_name=user_name,
            domain=domain,
            cloud_group=cloud_group,
            customization=customization,
            account_idx=account_idx,
            )
        self.email = '{user_name}+cloud-account-{cloud_group}-{customization}-{account_idx}@{domain}'.format(**params)
        self.first_name = 'Functional AutoTest #{account_idx}'.format(**params)
        self.last_name = 'for cloud group {cloud_group}, customization {customization}'.format(**params)


class CloudAccountFactory(object):

    def __init__(self, cloud_group, customization_name, cloud_host, autotest_email_password):
        self._cloud_group = cloud_group
        self._customization_name = customization_name
        self._cloud_host = cloud_host
        self._autotest_email_password = autotest_email_password
        self._account_idx = 0

    def __call__(self):
        self._account_idx += 1
        return self.create_cloud_account(self._account_idx)

    def create_cloud_account(self, account_idx):
        cloud_email = CloudEmail(self._cloud_group, self._customization_name, account_idx)
        cloud_account = CloudAccount(
            self._cloud_group, self._customization_name, self._cloud_host,
            cloud_email.email, CLOUD_ACCOUNT_PASSWORD)
        self.ensure_email_exists(cloud_account, cloud_email)
        return cloud_account

    def ensure_email_exists(self, cloud_account, cloud_email):
        _logger.info('Checking cloud account %r', cloud_email.email)
        try:
            user_info = cloud_account.get_user_info()
        except HttpError as x:
            if not x.json:
                raise x
            result_code = x.json.get('resultCode')
            assert result_code in ['badUsername', 'accountNotActivated'], repr(result_code)
            if not self._autotest_email_password:
                raise RuntimeError(
                    '--autotest-email-password must be provided to activate {!r}'.format(cloud_email.email))
            with IMAPConnection(IMAP_HOST, AUTOTEST_LOGIN_EMAIL, self._autotest_email_password) as imap_connection:
                imap_connection.delete_old_activation_messages(cloud_email.email)
                if result_code == 'badUsername':
                    _logger.info('Account %r is missing, creating new one', cloud_email.email)
                    cloud_account.register_user(cloud_email.first_name, cloud_email.last_name)
                else:
                    cloud_account.resend_activation_code()
                code = imap_connection.fetch_activation_code(
                    cloud_account.hostname,
                    cloud_email.email,
                    FETCH_ACTIVATION_EMAIL_TIMEOUT)
            cloud_account.activate_user(code)
            user_info = cloud_account.get_user_info()
        assert user_info.get('statusCode') == 'activated'
        if user_info['customization'] != self._customization_name:
            _logger.info('Account %r has wrong customization: %r; updating', cloud_email.email, user_info['customization'])
            cloud_account.set_user_customization(self._customization_name)
            user_info = cloud_account.get_user_info()
            assert user_info['customization'] == self._customization_name, repr(user_info)
        _logger.info(
            'Email %r for cloud group %r is valid and belongs to customization %r',
            cloud_email.email, self._cloud_group, self._customization_name)


def resolve_cloud_host_from_registry(cloud_group, customization):
    _logger.info(
        'Resolving cloud host for cloud group %r, customization %r on %r:',
        cloud_group, customization, CLOUD_HOST_REGISTRY_URL)
    response = requests.get(
        CLOUD_HOST_REGISTRY_URL,
        params=dict(group=cloud_group, vms_customization=customization),
        timeout=120)
    response.raise_for_status()
    cloud_host = response.content
    _logger.info('Resolved cloud host for cloud group %r, customization %r: %r', cloud_group, customization, cloud_host)
    return cloud_host
