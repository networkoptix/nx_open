import pytest

from framework.merging import setup_local_system


@pytest.fixture()
def mediaserver(one_mediaserver, updates_server_url_from_there):
    one_mediaserver.installation.update_mediaserver_conf({'checkForUpdateUrl': updates_server_url_from_there})
    one_mediaserver.start(already_started_ok=False)
    setup_local_system(one_mediaserver, {})
    return one_mediaserver


@pytest.fixture()
def updates_server_url_from_there(one_mediaserver, updates_server):
    address_from_here, port_from_here = updates_server
    port_from_there = one_mediaserver.os_access.port_map.local.tcp(port_from_here)
    address_from_there = one_mediaserver.os_access.port_map.local.address
    url = 'http://{}:{}'.format(address_from_there, port_from_there)
    return url
