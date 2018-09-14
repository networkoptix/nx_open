import pytest


@pytest.fixture()
def api(one_running_mediaserver):
    return one_running_mediaserver.api


@pytest.mark.parametrize(
    'url',
    [
        'api/ping',
        'api/systemSettings',
        'ec2/getFullInfo',
        'ec2/getUsers',
        'ec2/getCameras',
        ],
    ids=lambda path: path.replace('/', '_'))
def test_api_request(api, url):
    api.generic.get(url)
