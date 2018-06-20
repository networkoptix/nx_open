import socket

import pytest

from framework.api_shortcuts import get_updates_state
from framework.port_reservation import reserve_port
from framework.waiting import ensure_persistence
from updates_server.server import make_base_url_for_remote_machine


@pytest.fixture()
def updates_server(one_mediaserver):
    """Server which has been only bound to protect port from being bound by someone else"""
    address = 'localhost'
    s = socket.socket()

    def bind(port):
        s.bind((address, port))

    port, _ = reserve_port(range(8081, 8100), bind)

    yield make_base_url_for_remote_machine(one_mediaserver.os_access, port), []
    s.close()


def test_server_unavailable(mediaserver):
    ensure_persistence(
        lambda: get_updates_state(mediaserver.api) in {'notAvailable', 'checking'},
        "{} reports is checking for updates or reports they're not available".format(mediaserver))
    assert not mediaserver.installation.list_core_dumps()
