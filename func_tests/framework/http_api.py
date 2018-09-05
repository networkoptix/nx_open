import base64
import hashlib
import json
from abc import ABCMeta, abstractmethod
from pprint import pformat

import requests
import requests.exceptions
from requests.auth import HTTPDigestAuth
from urllib3.util import Url

from .switched_logging import SwitchedLogger

_logger = SwitchedLogger(__name__, 'http')


STANDARD_PASSWORDS = ['admin', 'qweasd123']  # do not mask these passwords in log files
REST_API_TIMEOUT_SEC = 20


class HttpError(Exception):
    """Error on HTTP or connection."""

    def __init__(self, server_name, url, status_code, reason, json=None):
        super(HttpError, self).__init__(
            self, '[%d] HTTP Error: %r for server %s url: %s' % (status_code, reason, server_name, url))
        self.status_code = status_code
        self.reason = reason
        self.json = json


class HttpClient(object):
    def __init__(self, base_url, ca_cert=None):
        self._base_url = base_url  # type: Url
        self.ca_cert = ca_cert
        if self.ca_cert is not None:
            _logger.info("Trust CA cert: %s.", self.ca_cert)
        username, password = self._base_url.auth.split(':', 1)
        self._auth = HTTPDigestAuth(username, password)
        self.user = username  # Only for interface.
        self.password = password  # Only for interface.

    def __repr__(self):
        assert isinstance(self._base_url, Url)
        # noinspection PyProtectedMember
        return "<HttpClient {}>".format(self._base_url._replace(auth=None))

    def set_credentials(self, username, password):
        """If credentials were changed"""
        self.user = username
        self.password = password
        # noinspection PyProtectedMember
        self._base_url = self._base_url._replace(auth='{}:{}'.format(username, password))
        self._auth = HTTPDigestAuth(username, password)

    def auth_key(self, method, path, realm, nonce):
        # `requests.auth.HTTPDigestAuth.build_digest_header` does the same but it substitutes empty path with '/'.
        # No straightforward way of getting key has been found.
        # This method is used only for specific tests, so there is no need to save one HTTP request.
        ha1 = hashlib.md5(':'.join([self.user.lower(), realm, self.password]).encode()).hexdigest()
        ha2 = hashlib.md5(':'.join([method, path]).encode()).hexdigest()  # Empty path.
        digest = hashlib.md5(':'.join([ha1, nonce, ha2]).encode()).hexdigest()
        key = base64.b64encode(':'.join([self.user.lower(), nonce, digest]).encode())
        return key.decode('ascii')

    def url(self, path):
        # noinspection PyProtectedMember
        return self._base_url._replace(auth=None, path='/' + path.lstrip('/')).url

    def secure_url(self, path):
        # noinspection PyProtectedMember
        return self._base_url._replace(scheme='https', auth=None, path='/' + path.lstrip('/')).url

    def media_url(self, path):
        # noinspection PyProtectedMember
        return self._base_url._replace(scheme='rtsp', path='/' + path.lstrip('/')).url

    def request(self, method, path, timeout=None, **kwargs):
        # noinspection PyProtectedMember
        response = requests.request(
            method, self._base_url._replace(path='/' + path.lstrip('/')),
            auth=self._auth,
            verify=str(self.ca_cert),
            allow_redirects=False,
            timeout=timeout or REST_API_TIMEOUT_SEC,
            **kwargs)
        return response


class HttpApi(object):
    __metaclass__ = ABCMeta

    def __init__(self, alias, http_client):  # type: (str, HttpClient) -> None
        self.alias = alias
        self.http = http_client

    @abstractmethod
    def request(self, method, path, timeout=None, **kwargs):
        return {}

    def get(self, path, params=None, **kwargs):
        params_str = pformat(params, indent=4)
        if '\n' in params_str or len(params_str) > 60:
            params_str = '\n' + params_str
        _logger.debug('GET %s, params: %s', self.http.url(path), params_str)
        assert 'data' not in kwargs
        assert 'json' not in kwargs
        return self.request('GET', path, params=params, **kwargs)

    def post(self, path, data, **kwargs):
        data_str = json.dumps(data)
        if len(data_str) > 60:
            data_str = '\n' + json.dumps(data, indent=4)
        _logger.debug('POST %s, payload: %s', self.http.url(path), data_str)
        return self.request('POST', path, json=data, **kwargs)
