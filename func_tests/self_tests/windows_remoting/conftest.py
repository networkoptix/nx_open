import pytest
import winrm

from framework.os_access.windows_remoting import WinRM
from framework.vms.hypervisor import obtain_running_vm
from framework.waiting import wait_for_true


@pytest.fixture(scope='session')
def windows_vm_info(configuration, hypervisor, vm_registries):
    windows_vm_registry = vm_registries['windows']
    with windows_vm_registry.taken('raw-windows') as (vm_index, vm_name):
        yield obtain_running_vm(hypervisor, vm_name, vm_index, configuration['vm_types']['windows']['vm'])


@pytest.fixture(scope='session')
def winrm(windows_vm_info):
    address, port = windows_vm_info.ports['tcp', 5985]
    winrm = WinRM(address, port, u'Administrator', u'qweasd123')
    wait_for_true(winrm.is_working, "{} is working".format(winrm))
    return winrm


@pytest.fixture(scope='session')
def pywinrm_protocol(windows_vm_info, username='Administrator', password='qweasd123'):
    address, port = windows_vm_info.ports['tcp', 5985]
    return winrm.Protocol(
        'http://{address}:{port}/wsman'.format(address=address, port=port),
        username=username, password=password,
        transport='ntlm')
