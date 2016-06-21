import requests
from requests.auth import HTTPDigestAuth
from cloud import settings
# from api.helpers.exceptions import validate_response
import urlparse

CLOUD_GATEWAY_URL = settings.CLOUD_CONNECT['gateway']


# @validate_response
def get(system_id, url, email=None, password=None):
    request = urlparse.urljoin(CLOUD_GATEWAY_URL, system_id, url)
    auth = None
    if email:
        auth = HTTPDigestAuth(email, password)

    return requests.get(request, auth=auth)


# @validate_response
def post(system_id, url, data, email=None, password=None):
    request = urlparse.urljoin(CLOUD_GATEWAY_URL, system_id, url)
    auth = None
    if email:
        auth = HTTPDigestAuth(email, password)

    return requests.post(request, json=data, auth=auth)
