import logging

import pytest
from pathlib2 import Path

from framework.vms.factory import VMFactory
from framework.vms.hypervisor import VMNotFound
from framework.vms.vm_type import VMType

_logger = logging.getLogger(__name__)


@pytest.fixture()
def vm_type(hypervisor):
    registry_path = Path('/tmp/func_tests/linux-test_factory.registry.yaml')
    if registry_path.exists():
        registry_path.unlink()
    vm_type = VMType(
        hypervisor,
        registry_path,
        'func_tests-temp-factory_test-{vm_index}',
        2,
        'trusty-template',
        '0A-00-00-FF-{vm_index:02X}-0{nic_index:01X}',
        {
            'host_ports_base': 39000,
            'host_ports_per_vm': 1,
            'forwarded_ports': {
                'ssh': {
                    'guest_port': 22,
                    'host_port_offset': 0,
                    'protocol': 'tcp',
                    },
                },
            },
        )
    for index, name in vm_type.registry.possible_entries():
        try:
            hypervisor.destroy(name)
        except VMNotFound:
            _logger.info("VM %r doesn't exist in %r.", name, hypervisor)
        else:
            _logger.info("VM %r removed from %r.", name, hypervisor)
    return vm_type


_vm_type = 'linux-test_factory'


@pytest.fixture()
def vm_factory(hypervisor, vm_type):
    vm_type_configuration = {'os_family': 'linux', 'power_on_timeout_sec': 120}
    factory = VMFactory({_vm_type: vm_type_configuration}, hypervisor, {_vm_type: vm_type})
    return factory


@pytest.mark.parallel_unsafe
def test_allocate(vm_factory):
    vm_alias = 'single'
    with vm_factory.allocated_vm(vm_alias, vm_type=_vm_type) as vm:
        assert vm.alias == vm_alias
        assert vm.index in {1, 2}
        assert vm.os_access.is_accessible()


@pytest.mark.parallel_unsafe
def test_allocate_two(vm_factory):
    with vm_factory.allocated_vm('a', vm_type=_vm_type) as a:
        with vm_factory.allocated_vm('b', vm_type=_vm_type) as b:
            assert a.name != b.name
            assert a.index != b.index


@pytest.mark.parallel_unsafe
def test_cleanup(vm_factory, hypervisor):
    with vm_factory.allocated_vm('a', vm_type=_vm_type) as a:
        with vm_factory.allocated_vm('b', vm_type=_vm_type) as b:
            vm_names = {a.name, b.name}
            _logger.info("Check VMs: %r. When allocated, VMs must exist.", vm_names)
            assert vm_names <= set(hypervisor.list_vm_names())
    _logger.info(
        "Existing VMs: %r. When not allocated, VMs are not required to exist.",
        vm_names & set(hypervisor.list_vm_names()))
    _logger.info("Cleanup VMs: %r.", vm_names)
    vm_factory.cleanup()
    _logger.info(
        "Existing VMs: %r. When cleaned up, VMs must not exist.",
        vm_names & set(hypervisor.list_vm_names()))
    assert not vm_names & set(hypervisor.list_vm_names())
