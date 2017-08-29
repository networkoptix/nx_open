import logging
import requests
from .server_rest_api import CloudRestApi


log = logging.getLogger(__name__)


CLOUD_USER_NAME = 'anikitin@networkoptix.com'
CLOUD_USER_PASSWORD ='qweasd123'

CLOUD_HOST_REGISTRY_URL = 'https://ireg.hdw.mx/api/v1/cloudhostfinder/'
CLOUD_HOST_REGISTRY_TIMEOUT_SEC = 120



class ServerBindInfo(object):

    def __init__(self, auth_key, system_id):
        self.auth_key = auth_key
        self.system_id = system_id


class CloudHost(object):

    def __init__(self, name, customization, url, user, password):
        self.name = name
        self.customization = customization
        self.url = url
        self.user = user
        self.password = password
        self.rest_api = CloudRestApi('cloud-host:%s' % name, url, user, password)

    def __repr__(self):
        return '%r @ %r' % (self.name, self.url)

    def ping(self):
        unused_realm = self.rest_api.cdb.ping.GET()

    def check_user_is_valid(self):
        unused_user_data = self.rest_api.cdb.account.get.GET()

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
    url = 'http://%s/' % host
    return CloudHost(cloud_group, customization, url, CLOUD_USER_NAME, CLOUD_USER_PASSWORD)
