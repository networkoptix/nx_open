import logging

import pytest
from pathlib2 import Path

from framework.os_access.local import LocalAccess
from framework.registry import Registry
from framework.vms.factory import VMFactory
from framework.vms.hypervisor import VMNotFound

_logger = logging.getLogger(__name__)


@pytest.fixture()
def registry(hypervisor):
    registry_path = Path('/tmp/func_tests/linux-test_factory.registry.yaml')
    registry_path.unlink()
    registry = Registry(LocalAccess(), registry_path, 'func_tests-temp-factory_test-{index}', 2)
    for index, name in registry.possible_entries():
        try:
            hypervisor.destroy(name)
        except VMNotFound:
            _logger.info("VM %r doesn't exist in %r.", name, hypervisor)
        else:
            _logger.info("VM %r removed from %r.", name, hypervisor)
    return registry


@pytest.fixture()
def vm_type_configuration():
    return {
        'os_family': 'linux',
        'power_on_timeout_sec': 60,
        'vm': {
            'mac_address_format': '0A-00-00-FF-{vm_index:02X}-0{nic_index:01X}',
            'port_forwarding': {
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
            'template_vm': 'trusty-template',
            'template_vm_snapshot': 'template',
            },
        }


_vm_type = 'linux-test_factory'


@pytest.fixture()
def vm_factory(hypervisor, vm_type_configuration, registry):
    factory = VMFactory({_vm_type: vm_type_configuration}, hypervisor, {_vm_type: registry})
    return factory


@pytest.mark.parallel_unsafe
def test_allocate(vm_factory):
    vm_alias = 'single'
    with vm_factory.allocated_vm(vm_alias, vm_type=_vm_type) as vm:
        assert vm.alias == vm_alias
        assert vm.index in {1, 2}
        assert vm.os_access.is_working()


@pytest.mark.parallel_unsafe
def test_allocate_two(vm_factory):
    with vm_factory.allocated_vm('a', vm_type=_vm_type) as a:
        with vm_factory.allocated_vm('b', vm_type=_vm_type) as b:
            assert a.name != b.name
            assert a.index != b.index
