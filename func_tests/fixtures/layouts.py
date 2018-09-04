import pytest
from contextlib2 import ExitStack
from pathlib2 import Path

from framework.merging import setup_system
from framework.networking import setup_networks
from framework.serialize import load

_layout_files_dir = Path(__file__).with_name('layout_files')


@pytest.fixture()
def layout(layout_file):
    layout_path = _layout_files_dir / layout_file
    return load(layout_path.read_text())


@pytest.fixture()
def network(hypervisor, vm_types, layout):
    machine_types = layout.get('machines', {})
    with ExitStack() as stack:
        def allocate_vm(alias):
            type = machine_types.get(alias, 'linux')
            vm = stack.enter_context(vm_types[type].vm_ready(alias))
            return vm
        networks_structure = layout['networks']
        reachability = layout.get('reachability', {})
        vms, _ = setup_networks(allocate_vm, hypervisor, networks_structure, reachability)
    return vms


@pytest.fixture()
def system(mediaserver_allocation, network, layout):
    with ExitStack() as stack:
        def allocate_mediaserver(alias):
            vm = network[alias]
            server = stack.enter_context(mediaserver_allocation(vm))
            return server
        used_mediaservers = setup_system(allocate_mediaserver, layout['mergers'])
        yield used_mediaservers
