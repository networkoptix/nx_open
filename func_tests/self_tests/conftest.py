import pytest

from defaults import defaults
from framework.networking.linux import LinuxNetworking
from framework.networking.windows import WindowsNetworking
from framework.os_access.posix_shell import SSH
from framework.os_access.windows_remoting import WinRM
from framework.vms.factory import SSH_PRIVATE_KEY_PATH
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
def linux_vm_info(vm_types):
    with vm_types['linux'].obtained('raw-linux') as (info, index):
        return info


@pytest.fixture(scope='session')
def windows_vm_info(vm_types):
    with vm_types['linux'].obtained('raw-linux') as (info, index):
        return info


@pytest.fixture(scope='session')
def winrm_raw(windows_vm_info):
    address = windows_vm_info.port_map.remote.address
    port = windows_vm_info.port_map.remote.tcp(5985)
    return WinRM(address, port, u'Administrator', u'qweasd123')


@pytest.fixture(scope='session')
def winrm(winrm_raw):
    wait_for_true(winrm_raw.is_working)
    return winrm_raw


@pytest.fixture()
def winrm_shell(winrm):
    return winrm._shell()


@pytest.fixture(scope='session')
def ssh(linux_vm_info):
    port = linux_vm_info.port_map.remote.tcp(22)
    address = linux_vm_info.port_map.remote.address
    ssh = SSH(address, port, u'root', SSH_PRIVATE_KEY_PATH)
    wait_for_true(ssh.is_working)
    return ssh


@pytest.fixture(scope='session')
def windows_networking(windows_vm_info, winrm):
    return WindowsNetworking(winrm, windows_vm_info.macs)


@pytest.fixture(scope='session')
def linux_networking(linux_vm_info, ssh):
    return LinuxNetworking(ssh, linux_vm_info.macs)


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
def windows_macs(windows_vm_info):
    return windows_vm_info.macs


@pytest.fixture(scope='session')
def macs(request, vm_type):
    if vm_type == 'linux':
        return request.getfixturevalue('linux_vm_info').macs
    if vm_type == 'windows':
        return request.getfixturevalue('windows_macs')
    assert False
