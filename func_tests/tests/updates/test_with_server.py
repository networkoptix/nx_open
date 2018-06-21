import pytest

from framework.api_shortcuts import get_updates_state
from framework.installation.mediaserver import Mediaserver
from framework.serving import WsgiServer, make_base_url_for_remote_machine
from framework.waiting import wait_for_true
from updates_server.server import UpdatesServer


@pytest.fixture()
def updates_server(work_dir, one_mediaserver, cloud_group):
    # Mediaserver which is stopped,
    # only needed to know what's address host has
    # from machine, on which mediaserver is installed.
    data_dir = work_dir / 'updates'
    server = UpdatesServer(data_dir)
    app = server.make_app(True, 'support')

    wsgi_server = WsgiServer(app, range(8081, 8100))
    # When port is bound and it's known how to access server's address and port, generate.
    base_url = make_base_url_for_remote_machine(one_mediaserver.os_access, wsgi_server.port)
    server.generate_data(base_url, cloud_group)
    with wsgi_server.serving():
        yield base_url, server.callback_requests, server.download_requests


def test_updates_available(mediaserver):
    wait_for_true(
        lambda: get_updates_state(mediaserver.api) == 'available',
        "{} reports update is available".format(mediaserver))
    assert not mediaserver.installation.list_core_dumps()


def test_install_script_called(mediaserver, updates_server):  # type: (Mediaserver) -> None
    _, callback_requests, download_requests = updates_server
    wait_for_true(
        lambda: get_updates_state(mediaserver.api) == 'available',
        "{} reports update is available".format(mediaserver))
    mediaserver.api.post('api/updates2', {'action': 'download'})
    wait_for_true(
        lambda: download_requests,
        "{} callback called".format(mediaserver))
    wait_for_true(
        lambda: get_updates_state(mediaserver.api) == 'readyToInstall',
        "{} reports update is ready to be installed".format(mediaserver))
    mediaserver.api.post('api/updates2', {'action': 'install'})
    wait_for_true(
        lambda: callback_requests,
        "{} callback called".format(mediaserver))
    assert not mediaserver.installation.list_core_dumps()
