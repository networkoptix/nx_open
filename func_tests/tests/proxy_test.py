import logging

import pytest
from requests import Session
from requests.auth import HTTPDigestAuth

_logger = logging.getLogger(__name__)


@pytest.fixture()
def proxy(two_merged_mediaservers):
    first, second = two_merged_mediaservers
    return first


@pytest.fixture()
def proxy_headers(two_merged_mediaservers):
    first, second = two_merged_mediaservers
    target_guid = second.api.get_server_id()
    return {'X-server-guid': target_guid}


@pytest.mark.parametrize('iterations', [1, 2, 10])
def test_ping(iterations, proxy, proxy_headers):
    with Session() as session:
        for _ in range(iterations):
            session.get(proxy.api.generic.http.url('api/ping'), headers=proxy_headers).raise_for_status()
    assert not proxy.installation.list_core_dumps()


@pytest.mark.parametrize('iterations', [1, 2, 10])
def test_ping_alternate_addressee(proxy, proxy_headers, iterations):
    # When doing non-proxy request after proxy request,
    # connection sometimes is closed by MediaServer.
    with Session() as session:
        for _ in range(iterations):
            session.get(proxy.api.generic.http.url('api/ping')).raise_for_status()
            session.get(proxy.api.generic.http.url('api/ping'), headers=proxy_headers).raise_for_status()
    assert not proxy.installation.list_core_dumps()


def test_redirect(proxy, proxy_headers):
    auth = HTTPDigestAuth(proxy.api.generic.http.user, proxy.api.generic.http.password)
    with Session() as session:
        redirect_response = session.get(proxy.api.generic.http.url(''), auth=auth, headers=proxy_headers, allow_redirects=False)
        assert 300 <= redirect_response.status_code <= 399, "Expected 3xx but got: %r" % redirect_response
        follow_url = proxy.api.generic.http.url(redirect_response.headers['Location'])
        follow_response = session.get(follow_url, auth=auth, headers=proxy_headers, allow_redirects=False)
        follow_response.raise_for_status()
    assert not proxy.installation.list_core_dumps()


def test_404(proxy, proxy_headers):
    auth = HTTPDigestAuth(proxy.api.generic.http.user, proxy.api.generic.http.password)
    with Session() as session:
        response = session.get(proxy.api.generic.http.url('') + 'nonexistent', auth=auth, headers=proxy_headers)
        assert response.status_code == 404, "Expected 404 but got: %r" % response
    assert not proxy.installation.list_core_dumps()
