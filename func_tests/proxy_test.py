import logging

import pytest
from requests import Session
from requests.auth import HTTPDigestAuth

log = logging.getLogger(__name__)


@pytest.fixture()
def layout_file():
    return 'direct-merge_toward_requested.yaml'


@pytest.fixture()
def proxy(network):
    _, servers = network
    return servers['first']


@pytest.fixture()
def proxy_headers(network):
    _, servers = network
    target_guid = servers['second'].ecs_guid
    return {'X-server-guid': target_guid}


@pytest.mark.parametrize('iterations', [1, 2, 10])
def test_ping(iterations, proxy, proxy_headers):
    with Session() as session:
        for _ in range(iterations):
            session.get(proxy.rest_api_url + 'api/ping', headers=proxy_headers).raise_for_status()


@pytest.mark.parametrize('iterations', [1, 2, 10])
def test_ping_alternate_addressee(proxy, proxy_headers, iterations):
    with Session() as session:
        for _ in range(iterations):
            session.get(proxy.rest_api_url + 'api/ping').raise_for_status()
            session.get(proxy.rest_api_url + 'api/ping', headers=proxy_headers).raise_for_status()


def test_redirect(proxy, proxy_headers):
    auth = HTTPDigestAuth(proxy.user, proxy.password)
    with Session() as session:
        redirect_response = session.get(proxy.rest_api_url, auth=auth, headers=proxy_headers, allow_redirects=False)
        assert 300 <= redirect_response.status_code <= 399, "Expected 3xx but got: %r" % redirect_response
        follow_url = proxy.rest_api_url.rstrip('/') + redirect_response.headers['Location']
        follow_response = session.get(follow_url, auth=auth, headers=proxy_headers, allow_redirects=False)
        follow_response.raise_for_status()


def test_404(proxy, proxy_headers):
    auth = HTTPDigestAuth(proxy.user, proxy.password)
    with Session() as session:
        response = session.get(proxy.rest_api_url + 'nonexistent', auth=auth, headers=proxy_headers)
        assert response.status_code == 404, "Expected 404 but got: %r" % response
