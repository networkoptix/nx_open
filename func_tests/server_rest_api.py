'''Proxy object for accessing REST API.

Allows calls to REST API using notation:
    rest_api_object.api.manualCamera.status.get(arg1=1, arg2=2)
which automatically translated to
    GET /api/manualCamera/status?arg1=1&arg2=2
But for POST method keyword parameters are translated to json request body.
'''

import json
import logging
import warnings
import requests
import requests.exceptions
from requests.packages.urllib3.exceptions import InsecureRequestWarning
from requests.auth import HTTPDigestAuth


REST_API_USER = 'admin'
REST_API_PASSWORD = 'admin'
REST_API_TIMEOUT_SEC = 10

log = logging.getLogger(__name__)


class HttpError(RuntimeError):

    def __init__(self, server_name, url, status_code, reason):
         RuntimeError.__init__(self, '[%d] HTTP Error: %s for server %s url: %s' % (status_code, reason, server_name, url))
         self.status_code = status_code
         self.reason = reason

        
class ServerRestApiError(RuntimeError):

    def __init__(self, server_name, url, error, error_string):
        RuntimeError.__init__(self, 'Server %s at %s REST API request returned error: [%s] %s' % (server_name, url, error, error_string))
        self.error = error
        self.error_string = error_string


class ServerRestApiProxy(object):

    def __init__(self, server_name, url, user, password):
        self._server_name = server_name
        self._url = url
        self._user = user
        self._password = password

    def __getattr__(self, name):
        return ServerRestApiProxy(self._server_name, self._url + '/' + name, self._user, self._password)

    def get(self, raise_exception=True, timeout_sec=None, headers=None, **kw):
        log.debug('%s: GET %s %s', self._server_name, self._url, kw)
        params = {name: self._get_param_to_str(value) for name, value in kw.items()}
        return self._make_request(raise_exception, timeout_sec, requests.get, self._url, headers=headers, params=params)

    def post(self, raise_exception=True, timeout_sec=None, headers=None, **kw):
        log.debug('%s: POST %s %s', self._server_name, self._url, kw)
        return self._make_request(raise_exception, timeout_sec, requests.post, self._url, headers=headers, json=kw)

    def _get_param_to_str(self, value):
        if type(value) is bool:
            return value and 'true' or 'false'
        return str(value)

    def _make_request(self, raise_exception, timeout_sec, fn, url, *args, **kw):
        if timeout_sec is None:
            timeout_sec = REST_API_TIMEOUT_SEC
        try:
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', InsecureRequestWarning)
                response = fn(url, auth=HTTPDigestAuth(self._user, self._password), timeout=timeout_sec, verify=False, *args, **kw)
        except requests.exceptions.RequestException as x:
            log.debug('\t--> %s: %s', x.__class__.__name__, x)
            raise
        log.debug('\t--> [%d] %s', response.status_code, response.content)
        if raise_exception:
            if 400 <= response.status_code < 600:
                raise HttpError(self._server_name, url, response.status_code, response.reason)
            else:
                response.raise_for_status()
        json = response.json()
        if type(json) != type({}) or not raise_exception:
            return json
        error = json.get('error')
        if error is None:
            return json
        if int(error):
            log.debug('%s', json)
            raise ServerRestApiError(self._server_name, self._url, json['error'], json['errorString'])
        else:
            return json['reply']


class RestApiBase(object):

    def __init__(self, server_name, url, user, password):
        self.server_name = server_name
        self.url = url
        self.user = user
        self.password = password

    def _make_proxy(self, path):
        return ServerRestApiProxy(self.server_name, self.url + path, self.user, self.password)


class ServerRestApi(RestApiBase):

    @property
    def ec2(self):
        return self._make_proxy('ec2')

    @property
    def api(self):
        return self._make_proxy('api')


class CloudRestApi(RestApiBase):

    @property
    def cdb(self):
        return self._make_proxy('cdb')
