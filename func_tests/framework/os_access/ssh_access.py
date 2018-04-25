import datetime
import logging

import pytz
from pathlib2 import Path

from framework.os_access.args import sh_augment_script, sh_command_to_script
from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.local_access import LocalAccess
from framework.os_access.ssh_path import SSHPath
from framework.utils import RunningTime

_logger = logging.getLogger(__name__)


class SSHAccess(object):
    def __init__(self, hostname, port, config_path=Path(__file__).with_name('ssh_config')):
        self.hostname = hostname
        self.config_path = config_path
        self.port = port
        self.ssh_command = ['ssh', '-F', config_path, '-p', port]

        class _SSHPath(SSHPath):
            """SSHPath type for this connection. isinstance should be supported."""

            @staticmethod
            def _ssh_access():
                # TODO: Use weak ref to avoid circular dependencies.
                return self

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

    def set_time(self, new_time):  # type: (SSHAccess, datetime.datetime) -> RunningTime
        # TODO: Make a separate Time class.
        started_at = datetime.datetime.now(pytz.utc)
        self.run_command(['date', '--set', new_time.isoformat()])
        return RunningTime(new_time, datetime.datetime.now(pytz.utc) - started_at)

    def first_setup(self):
        """Run once when VM is just created."""
        self.run_sh_script(
            # language=Bash
            '''
                # Mediaserver may crash several times during one test, all cores are kept.
                CORE_PATTERN_FILE='/etc/sysctl.d/60-core-pattern.conf'
                echo 'kernel.core_pattern=core.%t.%p' > "$CORE_PATTERN_FILE"  # %t is timestamp, %p is pid.
                sysctl -p "$CORE_PATTERN_FILE"  # See: https://superuser.com/questions/
                apt-get update
                apt-get --assume-yes install gdb # Required to create stack traces from core dumps.
                ''')
