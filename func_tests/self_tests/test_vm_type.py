import pytest

from framework.os_access.exceptions import DoesNotExist
from framework.vms.vm_type import VMType


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
    hypervisor.create_dummy_vm(dummy_vm_name, exists_ok=True)
    hypervisor.export_vm(dummy_vm_name, template_vm_served)
    return 'file://{!s}'.format(template_vm_served.absolute())


@pytest.fixture()
def vm_type(hypervisor, node_dir, template_url):
    registry_path = node_dir / 'registry.yaml'
    vm_type = VMType(
        hypervisor,
        registry_path,
        'func_tests-temp-factory_test-{vm_index}',
        2,
        'test_vm_type-template',
        '0A-00-00-FF-{vm_index:02X}-0{nic_index:01X}',
        {
            'host_ports_base': 38000,
            'host_ports_per_vm': 1,
            'vm_ports_to_host_port_offsets': {},
            },
        template_url=template_url,
        )
    vm_type.cleanup()
    return vm_type


def test_obtain(vm_type):  # type: (VMType) -> None
    with vm_type.obtained('test-vm_type') as (vm_info, _):
        assert vm_info
