import datetime
import logging

import requests

from .imap import IMAPConnection
from .rest_api import HttpError, RestApi

log = logging.getLogger(__name__)


CLOUD_HOST_REGISTRY_URL = 'https://ireg.hdw.mx/api/v1/cloudhostfinder/'
CLOUD_HOST_REGISTRY_TIMEOUT_SEC = 120

IMAP_HOST = 'imap.gmail.com'
AUTOTEST_EMAIL_ALIAS = 'autotest@networkoptix.com'
AUTOTEST_LOGIN_EMAIL = 'service@networkoptix.com'
CLOUD_ACCOUNT_PASSWORD = 'qweasd123'

# timeout when waiting for activation email to appear in IMAP inbox
FETCH_ACTIVATION_EMAIL_TIMEOUT = datetime.timedelta(minutes=1)


class ServerBindInfo(object):

    def __init__(self, auth_key, system_id):
        self.auth_key = auth_key
        self.system_id = system_id


class CloudAccount(object):

    def __init__(self, name, customization, host, user, password):
        self.name = name
        self.customization = customization
        self.host = host
        self.rest_api = RestApi('cloud-host:%s' % name, self.url, user, password)

    def __repr__(self):
        return '%r @ %r' % (self.name, self.url)

    @property
    def user(self):
        return self.rest_api.user

    @property
    def password(self):
        return self.rest_api.password

    @property
    def url(self):
        return 'http://%s/' % self.host

    def ping(self):
        unused_realm = self.rest_api.cdb.ping.GET()

    def get_user_info(self):
        return self.rest_api.cdb.account.get.GET()

    def register_user(self, first_name, last_name):
        response = self.rest_api.api.account.register.POST(
            email=self.user,
            password=self.password,
            first_name=first_name,
            last_name=last_name,
            subscribe=False,
            )
        assert response == dict(resultCode='ok'), repr(response)

    def resend_activation_code(self):
        response = self.rest_api.cdb.account.reactivate.POST(email=self.user)
        assert response == dict(code=''), repr(response)

    def activate_user(self, activation_code):
        response = self.rest_api.cdb.account.activate.POST(code=activation_code)
        assert response.get('email') == self.user, repr(response)  # Got activation code for another user?

    def set_user_customization(self, customization):
        response = self.rest_api.cdb.account.update.POST(customization=customization)
        assert response.get('resultCode') == 'ok'

    def check_user_is_valid(self):
        user_data = self.get_user_info()
        assert user_data.get('statusCode') == 'activated'

    def check_is_ready_for_tests(self):
        log.debug('Checking cloud host %r...', self)
        self.ping()
        log.debug('Cloud host %r is up', self)
        self.check_user_is_valid()
        log.info('Cloud host %r is up and test user is valid', self)

    def bind_system(self, system_name):
        response = self.rest_api.cdb.system.bind.GET(
            name=system_name,
            customization=self.customization,
            )
        return ServerBindInfo(response['authKey'], response['id'])


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

    def __init__(self, cloud_group, customization, cloud_host, autotest_email_password):
        self._cloud_group = cloud_group
        self._customization = customization
        self._cloud_host = cloud_host
        self._autotest_email_password = autotest_email_password
        self._account_idx = 0

    def __call__(self):
        self._account_idx += 1
        return self.create_cloud_account(self._account_idx)

    def create_cloud_account(self, account_idx):
        cloud_email = CloudEmail(self._cloud_group, self._customization, account_idx)
        cloud_account = CloudAccount(self._cloud_group, self._customization, self._cloud_host, cloud_email.email, CLOUD_ACCOUNT_PASSWORD)
        self.ensure_email_exists(cloud_account, cloud_email)
        return cloud_account

    def ensure_email_exists(self, cloud_account, cloud_email):
        log.info('Checking cloud account %r', cloud_email.email)
        try:
            user_info = cloud_account.get_user_info()
        except HttpError as x:
            result_code = x.json.get('resultCode')
            assert result_code in ['notAuthorized', 'accountNotActivated'], repr(result_code)
            assert self._autotest_email_password, '--autotest-email-password must be provided to activate %r' % cloud_email.email
            with IMAPConnection(IMAP_HOST, AUTOTEST_LOGIN_EMAIL, self._autotest_email_password) as imap_connection:
                imap_connection.delete_old_activation_messages(cloud_email.email)
                if result_code == 'notAuthorized':
                    log.info('Account %r is missing, creating new one', cloud_email.email)
                    cloud_account.register_user(cloud_email.first_name, cloud_email.last_name)
                else:
                    cloud_account.resend_activation_code()
                code = imap_connection.fetch_activation_code(cloud_account.host, cloud_email.email, FETCH_ACTIVATION_EMAIL_TIMEOUT)
            cloud_account.activate_user(code)
            user_info = cloud_account.get_user_info()
        assert user_info.get('statusCode') == 'activated'
        if user_info['customization'] != self._customization:
            log.info('Account %r has wrong customization: %r; updating', cloud_email.email, user_info['customization'])
            cloud_account.set_user_customization(self._customization)
            user_info = cloud_account.get_user_info()
            assert user_info['customization'] == self._customization, repr(user_info)
        log.info('Email %r for cloud group %r is valid and belongs to customization %r',
            cloud_email.email, self._cloud_group, self._customization)


def resolve_cloud_host_from_registry(cloud_group, customization):
    log.info('Resolving cloud host for cloud group %r, customization %r on %r:', cloud_group, customization, CLOUD_HOST_REGISTRY_URL)
    response = requests.get(
        CLOUD_HOST_REGISTRY_URL,
        params=dict(group=cloud_group, vms_customization=customization),
        timeout=CLOUD_HOST_REGISTRY_TIMEOUT_SEC)
    response.raise_for_status()
    cloud_host = response.content
    log.info('Resolved cloud host for cloud group %r, customization %r: %r', cloud_group, customization, cloud_host)
    return cloud_host
