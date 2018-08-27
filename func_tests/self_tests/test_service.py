import pytest

from framework.installation.upstart_service import UpstartService
from framework.installation.windows_service import WindowsService
from framework.waiting import wait_for_true


@pytest.fixture()
def service(one_vm):
    """Services safe to start and stop"""
    if one_vm.type == 'linux':
        ssh = one_vm.os_access.ssh
        ssh.run_sh_script(
            # language=Bash
            '''
                echo exec sleep 3600 >/etc/init/func_tests-dummy.conf
                initctl reload-configuration
                '''
            )
        return UpstartService(ssh, 'func_tests-dummy')  # Uncomplicated Firewall isn't used.
    if one_vm.type == 'windows':
        return WindowsService(one_vm.os_access.winrm, 'Spooler')  # Spooler manages printers.
    raise ValueError("Unknown VM type {}".format(one_vm))


def test_stop_start(service):
    # Initial state can be either started or stopped.
    # Service should be in same state as it was before.
    # Both actions must be executed.
    if service.is_running():
        service.stop()
        wait_for_true(lambda: not service.is_running(), "service is stopped")
        service.start()
        wait_for_true(service.is_running)
    else:
        service.start()
        wait_for_true(service.is_running)
        service.stop()
        wait_for_true(lambda: not service.is_running(), "service is stopped")
