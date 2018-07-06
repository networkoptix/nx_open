import pytest
from pathlib2 import Path

from framework.merging import setup_system
from framework.networking import setup_networks
from framework.pool import ClosingPool
from framework.serialize import load

_layout_files_dir = Path(__file__).with_name('layout_files')


@pytest.fixture()
def layout(layout_file):
    layout_path = _layout_files_dir / layout_file
    return load(layout_path.read_text())


@pytest.fixture()
def network(hypervisor, vm_factory, layout):
    machine_types = layout.get('machines', {})
    with ClosingPool(vm_factory.allocated_vm, machine_types, 'linux') as machines_pool:
        networks_structure = layout['networks']
        reachability = layout.get('reachability', {})
        vms, _ = setup_networks(machines_pool, hypervisor, networks_structure, reachability)
    return vms


@pytest.fixture()
def system(network, mediaserver_factory, layout):
    with ClosingPool(mediaserver_factory.allocated_mediaserver, network, None) as mediaservers_pool:
        used_mediaservers = setup_system(mediaservers_pool, layout['mergers'])
        yield used_mediaservers
