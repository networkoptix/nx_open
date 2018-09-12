import pytest
from contextlib2 import ExitStack

from framework.merging import setup_system


@pytest.fixture(scope='session', params=[
    'three_flat_pull.net.yaml',
    'triangle_pull.net.yaml',
    'triangle.net.yaml',
    ])
def layout_file(request):
    return request.param


def test_merge_by_scheme(mediaserver_allocation, network, layout):
    with ExitStack() as stack:
        def allocate_mediaserver(alias):
            vm = network[alias]
            server = stack.enter_context(mediaserver_allocation(vm))
            return server

        setup_system(allocate_mediaserver, layout['mergers'])
