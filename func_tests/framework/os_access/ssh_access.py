import logging

from pathlib2 import Path

from framework.os_access.args import sh_augment_script, sh_command_to_script
from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.local_access import LocalAccess

_logger = logging.getLogger(__name__)


class SSHAccess(object):
    def __init__(self, hostname, port, config_path=Path(__file__).with_name('ssh_config')):
        self.hostname = hostname
        self.config_path = config_path
        self.port = port
        self.ssh_command = ['ssh', '-F', config_path, '-p', port]

        from framework.os_access.ssh_path import SSHPath  # Reverse dependency -- import locally.

        class _SSHPath(SSHPath):
            """SSHPath type for this connection. isinstance should be supported."""
            _ssh_access = self

        self.Path = _SSHPath

    def __repr__(self):
        return '<SSHAccess {} {}>'.format(sh_command_to_script(self.ssh_command), self.hostname)

    def run_command(self, command, input=None, cwd=None, timeout_sec=60, env=None):
        script = sh_command_to_script(command)
        output = self.run_sh_script(script, input=input, cwd=cwd, timeout_sec=timeout_sec, env=env)
        return output

    def run_sh_script(self, script, input=None, cwd=None, timeout_sec=60, env=None):
        augmented_script = sh_augment_script(script, cwd, env)
        _logger.debug("Run on %r:\n%s", self, augmented_script)
        output = LocalAccess().run_command(
            self.ssh_command + [self.hostname] + ['\n' + augmented_script + '\n'],
            input=input,
            timeout_sec=timeout_sec)
        return output

    def is_working(self):
        try:
            self.run_sh_script(':')
        except exit_status_error_cls(255):
            return False
        return True
