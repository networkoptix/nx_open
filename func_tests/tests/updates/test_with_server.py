import errno
import socket
import wsgiref.simple_server
from threading import Thread

import pytest

from framework.api_shortcuts import get_updates_state
from framework.waiting import wait_for_true
from updates_server.server import UpdatesServer


@pytest.fixture(scope='session')
def updates_server(work_dir):
    data_dir = work_dir / 'updates'
    server = UpdatesServer(data_dir)
    server.generate_data()
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
    thread = Thread(target=wsgi_server.serve_forever)
    thread.start()
    yield wsgi_server.server_address, wsgi_server.server_port
    wsgi_server.shutdown()
    thread.join()
    wsgi_server.server_close()


def test_updates_available(mediaserver):
    wait_for_true(
        lambda: get_updates_state(mediaserver.api) == 'available',
        "{} reports update is available".format(mediaserver))
    assert not mediaserver.installation.list_core_dumps()
