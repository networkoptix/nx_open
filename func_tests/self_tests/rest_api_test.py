import base64

import requests


def test_auth_key(running_linux_mediaserver):
    api = running_linux_mediaserver.api
    response = requests.get(api.url('api/systemSettings'), params={'auth': api.auth_key('GET')})
    assert response.status_code == 200


def test_auth_key_wrong_method(running_linux_mediaserver):
    api = running_linux_mediaserver.api
    response = requests.get(api.url('api/systemSettings'), params={'auth': api.auth_key('WRONG')})
    assert response.status_code == 401


def test_auth_key_blatantly_wrong(running_linux_mediaserver):
    api = running_linux_mediaserver.api
    nonce_realm_response = requests.get(api.url('api/getNonce')).json()
    auth_digest = base64.b64encode('admin' + ":" + nonce_realm_response['reply']['nonce'] + ":" + 'wrong')
    response = requests.get(api.url('api/systemSettings'), params={'auth': auth_digest})
    assert response.status_code == 401


def test_no_auth(running_linux_mediaserver):
    api = running_linux_mediaserver.api
    response = requests.get(api.url('api/systemSettings'))
    assert response.status_code == 401
