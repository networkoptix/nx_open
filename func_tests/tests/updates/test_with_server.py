import errno
import socket
import wsgiref.simple_server
from threading import Thread

import pytest

from framework.api_shortcuts import get_updates_state
from framework.installation.mediaserver import Mediaserver
from framework.waiting import wait_for_true
from updates_server.server import UpdatesServer, make_base_url_for_remote_machine


@pytest.fixture()
def updates_server(work_dir, one_mediaserver):
    # Mediaserver which is stopped,
    # only needed to know what's address host has
    # from machine, on which mediaserver is installed.
    data_dir = work_dir / 'updates'
    server = UpdatesServer(data_dir)
    app = server.make_app(True, 'support')
    for port in range(8081, 8100):
        try:
            wsgi_server = wsgiref.simple_server.make_server('localhost', port, app.wsgi_app)
        except socket.error as e:
            if e.errno == errno.EADDRINUSE:
                continue
            raise
        break
    else:
        raise RuntimeError("Cannot find available port.")
    base_url = make_base_url_for_remote_machine(one_mediaserver.os_access, port)
    # When port is bound and it's known how to access server's address and port, generate.
    server.generate_data(base_url)
    thread = Thread(target=wsgi_server.serve_forever)
    thread.start()
    yield base_url, server.callback_requests, server.download_requests
    wsgi_server.shutdown()
    thread.join()
    wsgi_server.server_close()


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
