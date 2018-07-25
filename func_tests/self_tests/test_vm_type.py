import logging

import pytest

from framework.os_access.exceptions import DoesNotExist
from framework.vms.hypervisor import VMNotFound
from framework.vms.vm_type import VMType

_logger = logging.getLogger(__name__)


@pytest.fixture()
def template_url(node_dir, hypervisor):
    template_vm_served_dir = node_dir / 'served_dir'
    template_vm_served = template_vm_served_dir / 'template_vm.ova'
    dummy_vm_name = 'func_tests-to_export'
    try:
        template_vm_served_dir.rmtree()
    except DoesNotExist:
        pass
    template_vm_served_dir.mkdir(parents=True)
    dummy_vm = hypervisor.create_dummy_vm(dummy_vm_name, exists_ok=True)
    dummy_vm.export(template_vm_served)
    return 'file://{!s}'.format(template_vm_served.absolute())


@pytest.fixture()
def vm_type(hypervisor, node_dir, template_url):
    registry_path = node_dir / 'registry.yaml'
    if registry_path.exists():
        registry_path.unlink()
    vm_type = VMType(
        hypervisor,
        'linux',
        120,
        registry_path,
        'func_tests-temp-factory_test-{vm_index}',
        2,
        'trusty-template',
        '0A-00-00-FF-{vm_index:02X}-0{nic_index:01X}',
        {
            'host_ports_base': 39000,
            'host_ports_per_vm': 1,
            'vm_ports_to_host_port_offsets': {
                'tcp/22': 0,
                },
            },
        template_url=template_url,
        )
    for index, name in vm_type.registry.possible_entries():
        try:
            vm = hypervisor.find_vm(name)
        except VMNotFound:
            _logger.info("VM %r doesn't exist in %r.", name, hypervisor)
            continue
        vm.destroy()
        _logger.info("VM %r removed from %r.", name, hypervisor)
    return vm_type


def test_obtain(vm_type):  # type: (VMType) -> None
    with vm_type.obtained('test-vm_type') as (vm_info, _):
        assert vm_info


@pytest.mark.parallel_unsafe
@pytest.mark.parametrize('template_url', [None], ids='no_template')
def test_allocate(vm_type):
    vm_alias = 'single'
    with vm_type.allocated_vm(vm_alias) as vm:
        assert vm.alias == vm_alias
        assert vm.index in {1, 2}
        assert vm.os_access.is_accessible()


@pytest.mark.parallel_unsafe
@pytest.mark.parametrize('template_url', [None], ids='no_template')
def test_allocate_two(vm_type):
    with vm_type.allocated_vm('a') as a:
        with vm_type.allocated_vm('b') as b:
            assert a.name != b.name
            assert a.index != b.index


@pytest.mark.parallel_unsafe
@pytest.mark.parametrize('template_url', [None], ids='no_template')
def test_cleanup(vm_type, hypervisor):
    with vm_type.allocated_vm('a') as a:
        with vm_type.allocated_vm('b') as b:
            vm_names = {a.name, b.name}
            _logger.info("Check VMs: %r. When allocated, VMs must exist.", vm_names)
            assert vm_names <= set(hypervisor.list_vm_names())
    _logger.info(
        "Existing VMs: %r. When not allocated, VMs are not required to exist.",
        vm_names & set(hypervisor.list_vm_names()))
    _logger.info("Cleanup VMs: %r.", vm_names)
    vm_type.cleanup()
    _logger.info(
        "Existing VMs: %r. When cleaned up, VMs must not exist.",
        vm_names & set(hypervisor.list_vm_names()))
    assert not vm_names & set(hypervisor.list_vm_names())
