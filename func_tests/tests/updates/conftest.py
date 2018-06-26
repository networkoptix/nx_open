import pytest

from framework.merging import setup_local_system


@pytest.fixture()
def mediaserver(one_mediaserver, updates_server):
    url, _, _ = updates_server
    one_mediaserver.installation.update_mediaserver_conf({'checkForUpdateUrl': url})
    one_mediaserver.start(already_started_ok=False)
    setup_local_system(one_mediaserver, {})
    return one_mediaserver
