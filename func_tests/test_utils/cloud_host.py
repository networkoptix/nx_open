import logging
import datetime
import requests
from .server_rest_api import HttpError, CloudRestApi
from .imap import IMAPConnection

log = logging.getLogger(__name__)


CLOUD_HOST_REGISTRY_URL = 'https://ireg.hdw.mx/api/v1/cloudhostfinder/'
CLOUD_HOST_REGISTRY_TIMEOUT_SEC = 120

IMAP_HOST = 'imap.gmail.com'
AUTOTEST_EMAIL_ALIAS = 'autotest@networkoptix.com'
AUTOTEST_LOGIN_EMAIL = 'service@networkoptix.com'
AUTOTEST_EMAIL_PASSWORD = 'kbnUk06boqBkwU'
CLOUD_ACCOUNT_PASSWORD = 'qweasd123'

# timeout when waiting for activation email to appear in IMAP inbox
FETCH_ACTIVATION_EMAIL_TIMEOUT = datetime.timedelta(minutes=1)


class ServerBindInfo(object):

    def __init__(self, auth_key, system_id):
        self.auth_key = auth_key
        self.system_id = system_id


class CloudHost(object):

    def __init__(self, name, customization, host, user, password):
        self.name = name
        self.customization = customization
        self.host = host
        self.user = user
        self.password = password
        self.rest_api = CloudRestApi('cloud-host:%s' % name, self.url, user, password)

    def __repr__(self):
        return '%r @ %r' % (self.name, self.url)

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


def make_test_email(cloud_group, customization):
    user_name, domain = AUTOTEST_EMAIL_ALIAS.split('@')
    email = '{}+cloud-account-{}-{}@{}'.format(user_name, cloud_group, customization, domain)
    return email

def make_test_email_name(cloud_group, customization):
    first_name = 'Functional AutoTest'
    last_name = 'for cloud group {}, customization {}'.format(cloud_group, customization)
    return (first_name, last_name)

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

def create_cloud_host(cloud_group, customization, host):
    cloud_email = make_test_email(cloud_group, customization)
    cloud_host = CloudHost(cloud_group, customization, host, cloud_email, CLOUD_ACCOUNT_PASSWORD)
    ensure_email_exists(cloud_group, customization, cloud_host, cloud_email)
    return cloud_host

def ensure_email_exists(cloud_group, customization, cloud_host, cloud_email):
    log.info('Checking cloud account %r', cloud_email)
    try:
        user_info = cloud_host.get_user_info()
    except HttpError as x:
        result_code = x.json.get('resultCode')
        assert result_code in ['notAuthorized', 'accountNotActivated'], repr(result_code)
        with IMAPConnection(IMAP_HOST, AUTOTEST_LOGIN_EMAIL, AUTOTEST_EMAIL_PASSWORD) as imap_connection:
            imap_connection.delete_old_activation_messages(cloud_email)
            if result_code == 'notAuthorized':
                log.info('Account %r is missing, creating new one', cloud_email)
                first_name, last_name = make_test_email_name(cloud_group, customization)
                cloud_host.register_user(first_name, last_name)
            else:
                cloud_host.resend_activation_code()
            code = imap_connection.fetch_activation_code(cloud_host.host, cloud_email, FETCH_ACTIVATION_EMAIL_TIMEOUT)
        cloud_host.activate_user(code)
        user_info = cloud_host.get_user_info()
    assert user_info.get('statusCode') == 'activated'
    if user_info['customization'] != customization:
        log.info('Account %r has wrong customization: %r; updating', cloud_email, user_info['customization'])
        cloud_host.set_user_customization(customization)
        user_info = cloud_host.get_user_info()
        assert user_info['customization'] == customization, repr(user_info)
    log.info('Email %r for cloud group %r is valid and belongs to customization %r', cloud_email, cloud_group, customization)
