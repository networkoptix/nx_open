import pytest


@pytest.fixture()
def mediaserver(one_mediaserver, updates_server):
    url, _, _ = updates_server
    one_mediaserver.installation.update_mediaserver_conf({'checkForUpdateUrl': url})
    one_mediaserver.start(already_started_ok=False)
    one_mediaserver.api.setup_local_system()
    return one_mediaserver


@pytest.fixture()
def mediaserver(one_mediaserver, updates_server):
    url, _, _ = updates_server
    one_mediaserver.installation.update_mediaserver_conf({'checkForUpdateUrl': url})
    one_mediaserver.start(already_started_ok=False)
    one_mediaserver.api.setup_local_system()
    return one_mediaserver
