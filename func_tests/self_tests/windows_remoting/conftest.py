import pytest
import winrm

from fixtures.vms import vm_types_configuration
from framework.os_access.windows_remoting import WinRM
from framework.vms.hypervisor import obtain_running_vm
from framework.waiting import wait_for_true


@pytest.fixture(scope='session')
def windows_vm_info(hypervisor, vm_registries):
    windows_vm_registry = vm_registries['windows']
    with windows_vm_registry.taken('raw-windows') as (vm_index, vm_name):
        yield obtain_running_vm(hypervisor, vm_name, vm_index, vm_types_configuration()['windows']['vm'])


@pytest.fixture(scope='session')
def winrm(windows_vm_info):
    address, port = windows_vm_info.ports['tcp', 5985]
    winrm = WinRM(address, port, u'Administrator', u'qweasd123')
    wait_for_true(winrm.is_working)
    return winrm
