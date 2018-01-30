"""Server controller

Setup/start/stop server on vagrant boxes, remote or local hosts.
"""

import abc

MEDIASERVER_DIR = 'opt/{customization_company_name}/mediaserver'
MEDIASERVER_SERVICE_NAME = '{customization_company_name}-mediaserver'
SERVER_CTL_TARGET_PATH = 'server_ctl.sh'


class ServerCtl(object):

    __metaclass__ = abc.ABCMeta

    # returns bool: True if server is running, False if it's not
    @abc.abstractmethod
    def get_state(self):
        pass

    @abc.abstractmethod
    def set_state(self, is_started):
        pass

    def is_running(self):
        return self.get_state() == True


class VagrantBoxServerCtl(ServerCtl):
    """Control mediaserver as Upstart service with `status`, `start` and `stop` shortcuts

    SystemV's Upstart was deprecated several years ago and is not used by Debian/Ubuntu anymore.
    These tests are running mostly on Ubuntu 14.04, that is stated as requirement.
    """
    def __init__(self, os_access, service_name):
        self.os_access = os_access
        self._service_name = service_name

    def get_state(self):
        output = self._run_service_action('status')
        goal = output.split()[1].split('/')[0]
        assert goal in ['start', 'stop'], repr(output.strip())
        return goal == 'start'

    def set_state(self, is_started):
        self._run_service_action('start' if is_started else 'stop')

    def make_core_dump(self):
        self.os_access.run_command(['killall', '--signal', 'SIGTRAP', 'mediaserver-bin'])

    def _run_service_action(self, action):
        return self.os_access.run_command([action, self._service_name])


class PhysicalHostServerCtl(ServerCtl):
    """Run multiple mediaservers on single machine

    Service of any kind doesn't can only run one instance.
    This functionality is used by scalability and load tests
    and is not intended to be used by end users.

    When multiple servers are installed,
    shell script is created from templates.
    Its interface mimic a `service` command.
    """
    def __init__(self, hostname, dir):
        self._os_access = hostname
        self._dir = dir

    def get_state(self):
        if not self._os_access.file_exists(self._server_ctl_path):
            return False  # not even installed
        return self._run_service_action('is_active') == 'active'

    def set_state(self, is_started):
        self._run_service_action('start' if is_started else 'stop')

    def make_core_dump(self):
        self._run_service_action('make_core_dump')

    def _run_service_action(self, action):
        return self._os_access.run_command([self._server_ctl_path, action]).strip()

    @property
    def _server_ctl_path(self):
        return self._dir / 'server_ctl.sh'
