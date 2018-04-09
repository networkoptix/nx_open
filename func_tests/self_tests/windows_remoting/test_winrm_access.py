from framework.os_access.windows_remoting.winrm_access import WinRMAccess


def test_winrm_access_works(windows_vm_info):
    hostname, port = windows_vm_info.ports['tcp', 5985]
    access = WinRMAccess(hostname, port)
    assert access.is_working()
