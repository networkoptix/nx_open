import base64
import logging
import time

import pytest
import requests

from framework import serving
from framework.mediaserver_api import GenericMediaserverApi, MediaserverApiRequestError

_logger = logging.getLogger(__name__)


def test_auth_key(one_running_mediaserver):
    api = one_running_mediaserver.api
    response = requests.get(api.generic.http.url('api/systemSettings'), params={'auth': api.auth_key('GET')})
    assert response.status_code == 200


def test_auth_key_wrong_method(one_running_mediaserver):
    api = one_running_mediaserver.api
    response = requests.get(api.generic.http.url('api/systemSettings'), params={'auth': api.auth_key('WRONG')})
    assert response.status_code == 401


def test_auth_key_blatantly_wrong(one_running_mediaserver):
    api = one_running_mediaserver.api
    nonce_realm_response = requests.get(api.generic.http.url('api/getNonce')).json()
    auth_digest = base64.b64encode('admin' + ":" + nonce_realm_response['reply']['nonce'] + ":" + 'wrong')
    response = requests.get(api.generic.http.url('api/systemSettings'), params={'auth': auth_digest})
    assert response.status_code == 401


def test_no_auth(one_running_mediaserver):
    api = one_running_mediaserver.api
    response = requests.get(api.generic.http.url('api/systemSettings'))
    assert response.status_code == 401


def test_connection_error(service_ports):
    with serving.reserved_port(service_ports[30:35]) as port:
        api = GenericMediaserverApi.new('not a server', '127.0.0.1', port)
        exception_info = pytest.raises(MediaserverApiRequestError, api.get, 'oi')
        _logger.info(exception_info)


def test_timeout(service_ports):
    def app(_, start_response):
        start_response('200 OK', [('Content-Type', 'text/plain')])
        yield b'Started.\n'
        time.sleep(2)
        yield b'Done.\n'

    server = serving.WsgiServer(app, service_ports[30:35])
    with server.serving():
        api = GenericMediaserverApi.new('not a server', '127.0.0.1', server.port)
        exception_info = pytest.raises(MediaserverApiRequestError, api.get, '', timeout=1)
        _logger.info(exception_info)

