import re
from io import StringIO
import platform
from vms_benchmark.device_tests import DeviceTestResult


class SshHostKeyIsKnown:
    def __init__(self, dev):
        self.dev = dev

    def call(self):
        if platform.system() == 'Windows':
            stderr = StringIO()
            res = self.dev.sh('true', stderr=stderr)

            error_messages = re.split(r'[\r\n]+', stderr.getvalue().strip())

            host_key = 'unneeded'

            if error_messages[-1] == 'Connection abandoned.':
                host_key = error_messages[-2].split()[-1]

            if not res or res.return_code != 0:
                return DeviceTestResult(success=False, message=str(error_messages))

            return DeviceTestResult(success=True, details=[host_key])
        else:
            return DeviceTestResult(success=True, details=['unneeded'])
