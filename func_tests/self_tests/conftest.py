import pytest

from defaults import defaults
from framework.networking.linux import LinuxNetworking
from framework.networking.windows import WindowsNetworking
from framework.os_access.ssh_shell import SSH
from framework.os_access.windows_remoting import WinRM
from framework.waiting import wait_for_true


def pytest_addoption(parser):
    parser.addoption(
        '--dummy-smb-url',
        default=defaults.get('dummy_smb_url'),
        help='Dummy smb:// url pointing to existing file.')


# Windows and Linux fixtures are made separate because they're used to get WinRM or SSH
# and then to use these as fixtures directly or create Networking objects.
# There is no simple way to do that with overriding.

@pytest.fixture(scope='session')
def linux_vm(vm_types):
    with vm_types['linux'].vm_started('raw-linux') as vm:
        return vm


@pytest.fixture(scope='session')
def windows_vm(vm_types):
    with vm_types['windows'].vm_started('raw-windows') as vm:
        return vm


@pytest.fixture(scope='session')
def winrm(windows_vm):
    winrm = windows_vm.os_access.winrm
    wait_for_true(winrm.is_working)
    return winrm


@pytest.fixture()
def winrm_shell(winrm):
    return winrm._shell()


@pytest.fixture(scope='session')
def ssh(linux_vm):
    ssh = linux_vm.os_access.ssh
    wait_for_true(ssh.is_working)
    return ssh


@pytest.fixture(scope='session')
def windows_networking(windows_vm):
    return windows_vm.os_access.networking


@pytest.fixture(scope='session')
def linux_networking(linux_vm):
    return linux_vm.os_access.networking


@pytest.fixture(scope='session', params=['linux', 'windows'])
def vm_type(request):
    return request.param


@pytest.fixture(scope='session')
def networking(request, vm_type):
    if vm_type == 'linux':
        return request.getfixturevalue('linux_networking')
    if vm_type == 'windows':
        return request.getfixturevalue('windows_networking')
    assert False


@pytest.fixture(scope='session')
def windows_macs(windows_vm):
    return windows_vm.macs


@pytest.fixture(scope='session')
def macs(request, vm_type):
    if vm_type == 'linux':
        return request.getfixturevalue('linux_vm').hardware.macs
    if vm_type == 'windows':
        return request.getfixturevalue('windows_macs')
    assert False
