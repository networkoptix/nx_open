import pytest

from defaults import defaults
from framework.os_access.posix_access import local_access


def pytest_addoption(parser):
    parser.addoption(
        '--dummy-smb-url',
        default=defaults.get('dummy_smb_url'),
        help='Dummy smb:// url pointing to existing file.')


@pytest.fixture()
def winrm(windows_vm):
    return windows_vm.os_access.winrm


@pytest.fixture()
def winrm_shell(winrm):
    return winrm._shell()


@pytest.fixture()
def ssh(linux_vm):
    return linux_vm.os_access.shell


@pytest.fixture(params=['linux', 'windows', 'local'])
def os_access(request):
    if request.param == 'local':
        return local_access
    else:
        vm_fixture = request.param + '_vm'
        vm = request.getfixturevalue(vm_fixture)
        return vm.os_access
