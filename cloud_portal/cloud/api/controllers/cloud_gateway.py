import requests
from requests.auth import HTTPDigestAuth
from cloud import settings
from api.helpers.exceptions import validate_mediaserver_response
import urlparse
import logging
logger = logging.getLogger(__name__)

CLOUD_GATEWAY_URL = settings.CLOUD_CONNECT['gateway']


@validate_mediaserver_response
def get(system_id, url, email=None, password=None):
    request = urlparse.urljoin(CLOUD_GATEWAY_URL, system_id+'/')
    request = urlparse.urljoin(request, url)
    auth = None
    if email:
        auth = HTTPDigestAuth(email, password)

    # logger.debug('get ' + url + ' proxy: ' + request)
    return requests.get(request, auth=auth)


@validate_mediaserver_response
def post(system_id, url, data, email=None, password=None):
    request = urlparse.urljoin(CLOUD_GATEWAY_URL, system_id+'/')
    request = urlparse.urljoin(request, url)
    auth = None
    if email:
        auth = HTTPDigestAuth(email, password)

    # logger.debug('post ' + url + ' proxy: ' + request)
    return requests.post(request, json=data, auth=auth)
