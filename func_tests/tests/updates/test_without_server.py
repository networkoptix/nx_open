import pytest

from framework.api_shortcuts import get_updates_state
from framework.serving import make_base_url_for_remote_machine, reserved_port
from framework.waiting import ensure_persistence


@pytest.fixture()
def updates_server(one_mediaserver):
    """Server which has been only bound to protect port from being bound by someone else"""
    with reserved_port(range(8081, 8100)) as port:
        yield make_base_url_for_remote_machine(one_mediaserver.os_access, port), []


def test_server_unavailable(mediaserver):
    ensure_persistence(
        lambda: get_updates_state(mediaserver.api) in {'notAvailable', 'checking'},
        "{} reports is checking for updates or reports they're not available".format(mediaserver))
    assert not mediaserver.installation.list_core_dumps()
