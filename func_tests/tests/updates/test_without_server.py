import errno
import socket

import pytest

from framework.api_shortcuts import get_updates_state
from framework.waiting import ensure_persistence


@pytest.fixture(scope='session')
def updates_server():
    """Server which has been only bound to protect port from being bound by someone else"""
    address = 'localhost'
    for port in range(8080, 8100):
        s = socket.socket()
        try:
            s.bind((address, port))
        except socket.error as e:
            if e.errno == errno.EADDRINUSE:
                continue
            raise
        break
    else:
        raise RuntimeError("Cannot find available port.")
    yield address, port
    s.close()


def test_server_unavailable(mediaserver):
    ensure_persistence(
        lambda: get_updates_state(mediaserver.api) in {'notAvailable', 'checking'},
        "{} reports is checking for updates or reports they're not available".format(mediaserver))
    assert not mediaserver.installation.list_core_dumps()
