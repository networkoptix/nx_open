import pytest

from framework.installation.bulk import many_mediaservers_allocated
from framework.merging import setup_system


@pytest.fixture(scope='session', params=[
    'three_flat_pull.net.yaml',
    'triangle_pull.net.yaml',
    'triangle.net.yaml',
    ])
def layout_file(request):
    return request.param


@pytest.fixture()
def mediaservers(mediaserver_allocation, network):
    hosts = {
        alias: machine
        for alias, machine in network.items()
        if not machine.os_access.networking.is_router()
        }
    with many_mediaservers_allocated(hosts, mediaserver_allocation) as mediaservers:
        yield mediaservers


def test_merge_by_scheme(mediaservers, layout):
    setup_system(mediaservers, layout['mergers'])
