import base64
import hashlib
import json
import logging
from abc import ABCMeta, abstractmethod
from pprint import pformat

import requests
import requests.exceptions
from requests.auth import HTTPDigestAuth

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
    def __init__(self, hostname, port, username, password, ca_cert=None):
        self._port = port
        self._hostname = hostname
        self.ca_cert = ca_cert
        if self.ca_cert is not None:
            _logger.info("Trust CA cert: %s.", self.ca_cert)
        self._auth = HTTPDigestAuth(username, password)
        self.user = username  # Only for interface.
        self.password = password  # Only for interface.

    def __repr__(self):
        password_display = self._auth.password if self._auth.password in STANDARD_PASSWORDS else '***'
        return "<HttpClient {} {}:{}>".format(self.url(''), self._auth.username, password_display)

    def set_credentials(self, username, password):
        """If credentials were changed"""
        self.user = username
        self.password = password
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

    def url(self, path, secure=False, media=False, with_auth=False):
        return '{}://{}{}:{}/{}'.format(
            'rtsp' if media else ('https' if secure else 'http'),
            '{}:{}@'.format(self.user, self.password) if with_auth else '',
            self._hostname, self._port, path.lstrip('/'))

    def request(self, method, path, secure=False, timeout=None, **kwargs):
        url = self.url(path, secure=secure)
        response = requests.request(
            method, url,
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
    def request(self, method, path, secure=False, timeout=None, **kwargs):
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
