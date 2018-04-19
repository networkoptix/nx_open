import pytest

from framework.os_access.windows_remoting.winrm_access import WinRMAccess
from framework.waiting import wait_for_true


@pytest.fixture()
def winrm_access_raw(windows_vm_info):
    hostname, port = windows_vm_info.ports['tcp', 5985]
    return WinRMAccess(hostname, port)


@pytest.fixture()
def winrm_access(winrm_access_raw):
    wait_for_true(winrm_access_raw.is_working, "{} is working".format(winrm_access))
    return winrm_access_raw


def test_construction(windows_vm_info):
    winrm_access_raw(windows_vm_info)


def test_is_working(winrm_access_raw):
    winrm_access(winrm_access_raw)
