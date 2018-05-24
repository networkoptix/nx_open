import logging

from framework.installation.service import Service
from framework.os_access.exceptions import NonZeroExitStatus
from framework.os_access.posix_shell import SSH

_logger = logging.getLogger(__name__)


class UpstartService(Service):
    """Control mediaserver as Upstart service with `status`, `start` and `stop` shortcuts

    SystemV's Upstart was deprecated several years ago and is not used by Debian/Ubuntu anymore.
    These tests are running mostly on Ubuntu 14.04, that is stated as requirement.
    """

    def __init__(self, ssh, service_name, start_timeout_sec=10, stop_timeout_sec=10):
        self._ssh = ssh  # type: SSH
        self._service_name = service_name
        self._start_timeout_sec = start_timeout_sec
        self._stop_timeout_sec = stop_timeout_sec

    def is_running(self):
        output = self._ssh.run_command(['status', self._service_name])
        return output.split()[1].split('/')[0] == 'start'

    def start(self):
        self._ssh.run_command(['start', self._service_name], timeout_sec=self._start_timeout_sec)

    def stop(self):
        self._ssh.run_command(['stop', self._service_name], timeout_sec=self._stop_timeout_sec)

    def make_core_dump(self):
        try:
            self._ssh.run_command(['killall', '--signal', 'SIGTRAP', 'mediaserver-bin'])
        except NonZeroExitStatus:
            _logger.error("Cannot make core dump of process of %s service on %r.", self._service_name, self._ssh)


class LinuxAdHocService(Service):
    """Run multiple mediaservers on single machine

    Service of any kind doesn't can only run one instance.
    This functionality is used by scalability and load tests
    and is not intended to be used by end users.

    When multiple servers are installed,
    shell script is created from templates.
    Its interface mimic a `service` command.
    """

    def __init__(self, ssh, dir):
        self._ssh = ssh
        self._service_script_path = dir / 'server_ctl.sh'

    def is_running(self):
        # TODO: Make a script.
        if not self._service_script_path.exists():
            return False  # not even installed
        output = self._ssh.run_command([self._service_script_path, 'is_active'])
        return output.strip() == 'active'

    def start(self):
        return self._ssh.run_command([self._service_script_path, 'start'])

    def stop(self):
        return self._ssh.run_command([self._service_script_path, 'stop'])

    def make_core_dump(self):
        self._ssh.run_command([self._service_script_path, 'make_core_dump'])
