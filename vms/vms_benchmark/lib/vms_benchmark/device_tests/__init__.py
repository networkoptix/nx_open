class DeviceTestResult:
    def __init__(self, success, message='', details=()):
        self.success = bool(success)
        self.message = str(message)
        self.details = list(details)

    def formatted_message(self):
        return self.message % self.details

from .ssh_host_key_is_known import SshHostKeyIsKnown
from .sudo_configured import SudoConfigured
