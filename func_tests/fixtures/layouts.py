import pytest
from pathlib2 import Path

from framework.installation.bulk import many_mediaservers_allocated
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
    hosts = [machine for machine in network if not machine.is_router()]
    with many_mediaservers_allocated(hosts, mediaserver_allocation) as mediaservers:
        setup_system(mediaservers, layout['mergers'])
        yield mediaservers
