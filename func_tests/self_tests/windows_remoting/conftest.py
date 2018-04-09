import pytest
import winrm

from framework.vms.hypervisor import obtain_running_vm


@pytest.fixture()
def windows_vm_info(configuration, hypervisor, vm_registries):
    windows_vm_registry = vm_registries['windows']
    with windows_vm_registry.taken('raw-windows') as (vm_index, vm_name):
        yield obtain_running_vm(hypervisor, vm_name, vm_index, configuration['vm_types']['windows']['vm'])


@pytest.fixture()
def pywinrm_protocol(windows_vm, username='Administrator', password='qweasd123'):
    address, port = windows_vm.ports['tcp', 5985]
    return winrm.Protocol(
        'http://{address}:{port}/wsman'.format(address=address, port=port),
        username=username, password=password,
        transport='ntlm')
