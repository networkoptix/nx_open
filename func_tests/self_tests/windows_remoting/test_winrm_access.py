import pytest

from framework.os_access.windows_remoting import WinRM
from framework.waiting import wait_for_true


@pytest.fixture()
def winrm_raw(windows_vm_info):
    address, port = windows_vm_info.ports['tcp', 5985]
    return WinRM(address, port, u'Administrator', u'qweasd123')


@pytest.fixture()
def winrm(winrm_raw):
    wait_for_true(winrm_raw.is_working)
    return winrm_raw


def test_construction(windows_vm_info):
    winrm_raw(windows_vm_info)


def test_is_working(winrm_raw):
    winrm(winrm_raw)
