import abc
import logging

from framework.os_access.exceptions import NonZeroExitStatus

_logger = logging.getLogger(__name__)


class Service(object):
    __metaclass__ = abc.ABCMeta

    # returns bool: True if server is running, False if it's not
    @abc.abstractmethod
    def is_running(self):
        pass

    @abc.abstractmethod
    def start(self):
        pass

    @abc.abstractmethod
    def stop(self):
        pass


class UpstartService(Service):
    """Control mediaserver as Upstart service with `status`, `start` and `stop` shortcuts

    SystemV's Upstart was deprecated several years ago and is not used by Debian/Ubuntu anymore.
    These tests are running mostly on Ubuntu 14.04, that is stated as requirement.
    """

    def __init__(self, os_access, service_name):
        self.os_access = os_access
        self._service_name = service_name

    def is_running(self):
        output = self.os_access.run_command(['status', self._service_name])
        return output.split()[1].split('/')[0] == 'start'

    def start(self):
        self.os_access.run_command(['start', self._service_name])

    def stop(self):
        self.os_access.run_command(['stop', self._service_name])

    def make_core_dump(self):
        try:
            self.os_access.run_command(['killall', '--signal', 'SIGTRAP', 'mediaserver-bin'])
        except NonZeroExitStatus:
            _logger.error("Cannot make core dump of process of %s service on %r.", self._service_name, self.os_access)


class AdHocService(Service):
    """Run multiple mediaservers on single machine

    Service of any kind doesn't can only run one instance.
    This functionality is used by scalability and load tests
    and is not intended to be used by end users.

    When multiple servers are installed,
    shell script is created from templates.
    Its interface mimic a `service` command.
    """

    def __init__(self, os_access, dir):
        self._os_access = os_access
        self._service_script_path = dir / 'server_ctl.sh'

    def is_running(self):
        # TODO: Make a script.
        if not self._service_script_path.exists():
            return False  # not even installed
        output = self._os_access.run_command([self._service_script_path, 'is_active'])
        return output.strip() == 'active'

    def start(self):
        return self._os_access.run_command([self._service_script_path, 'start'])

    def stop(self):
        return self._os_access.run_command([self._service_script_path, 'stop'])

    def make_core_dump(self):
        self._os_access.run_command([self._service_script_path, 'make_core_dump'])
