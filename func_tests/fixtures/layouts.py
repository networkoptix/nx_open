import pytest
from contextlib2 import ExitStack
from pathlib2 import Path

from framework.merging import setup_system
from framework.serialize import load
from framework.vms.bulk import many_allocated
from framework.vms.networks import setup_networks

_layout_files_dir = Path(__file__).with_name('layout_files')


@pytest.fixture()
def layout(layout_file):
    layout_path = _layout_files_dir / layout_file
    return load(layout_path.read_text())


@pytest.fixture()
def network(hypervisor, vm_types, layout):
    with many_allocated(vm_types, layout['machines']) as machines:
        reachability = layout.get('reachability', {})
        setup_networks(machines, hypervisor, layout['networks'], reachability)
        yield machines


@pytest.fixture()
def system(mediaserver_allocation, network, layout):
    with ExitStack() as stack:
        def allocate_mediaserver(alias):
            vm = network[alias]
            server = stack.enter_context(mediaserver_allocation(vm))
            return server
        used_mediaservers = setup_system(allocate_mediaserver, layout['mergers'])
        yield used_mediaservers
