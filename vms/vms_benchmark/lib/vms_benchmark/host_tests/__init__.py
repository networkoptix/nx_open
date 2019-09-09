class HostTestResult:
    def __init__(self, success, message='', details=()):
        self.success = bool(success)
        self.message = str(message)
        self.details = list(details)

    def formatted_message(self):
        return self.message % self.details

from .ssh_pass_installed import SshPassInstalled
