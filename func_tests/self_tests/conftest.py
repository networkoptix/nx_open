import pytest

from defaults import defaults


def pytest_addoption(parser):
    parser.addoption(
        '--dummy-smb-url',
        default=defaults.get('dummy_smb_url'),
        help='Dummy smb:// url pointing to existing file.')


@pytest.fixture(scope='session')
def winrm(windows_vm):
    return windows_vm.os_access.winrm


@pytest.fixture()
def winrm_shell(winrm):
    return winrm._shell()


@pytest.fixture(scope='session')
def ssh(linux_vm):
    return linux_vm.os_access.ssh
