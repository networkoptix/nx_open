import os

from vms_benchmark.host_tests import HostTestResult


class SshPassInstalled:
    @staticmethod
    def call():
        if os.system('which sshpass >/dev/null 2>&1') != 0:
            return HostTestResult(
                success=False,
                message="Command `which sshpass` failed."
            )
        return HostTestResult(success=True)
