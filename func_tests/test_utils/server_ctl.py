'Server controller: setup/start/stop server on vagrant boxes, remote or local hosts'

import abc
import os.path

from .host import Host

MEDIASERVER_DIR = 'opt/{customization_company_name}/mediaserver'
MEDIASERVER_SERVICE_NAME = '{customization_company_name}-mediaserver'
SERVER_CTL_TARGET_PATH = 'server_ctl.sh'


class ServerCtl(object):

    __metaclass__ = abc.ABCMeta

    @property
    @abc.abstractmethod
    def host(self):
        pass

    @property
    @abc.abstractmethod
    def server_dir(self):
        pass

    @property
    @abc.abstractmethod
    def host_timezone(self):
        pass

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

    def __init__(self, customization_company_name, box):
        self._customization_company_name = customization_company_name
        self._box = box
        self._service_name = MEDIASERVER_SERVICE_NAME.format(
            customization_company_name=self._customization_company_name)

    @property
    def host(self):
        return self._box.host

    @property
    def server_dir(self):
        return os.path.join('/', MEDIASERVER_DIR.format(customization_company_name=self._customization_company_name))

    @property
    def host_timezone(self):
        return self._box.timezone

    def get_state(self):
        output = self._run_service_action('status')
        goal = output.split()[1].split('/')[0]
        assert goal in ['start', 'stop'], repr(output.strip())
        return goal == 'start'

    def set_state(self, is_started):
        self._run_service_action('start' if is_started else 'stop')

    def make_core_dump(self):
        self.host.run_command(['killall', '--signal', 'SIGTRAP', 'mediaserver-bin'])

    def _run_service_action(self, action):
        return self.host.run_command([action, self._service_name])


class PhysicalHostServerCtl(ServerCtl):

    def __init__(self, host, dir):
        assert isinstance(host, Host), repr(host)
        self._host = host
        self._dir = dir

    @property
    def host(self):
        return self._host

    @property
    def server_dir(self):
        return self._dir

    @property
    def host_timezone(self):
        return self._host.get_timezone()

    def get_state(self):
        if not self._host.file_exists(self._server_ctl_path):
            return False  # not even installed
        return self._run_service_action('is_active') == 'active'

    def set_state(self, is_started):
        self._run_service_action('start' if is_started else 'stop')

    def make_core_dump(self):
        self._run_service_action('make_core_dump')

    def _run_service_action(self, action):
        return self._host.run_command([self._server_ctl_path, action]).strip()

    @property
    def _server_ctl_path(self):
        return os.path.join(self._dir, 'server_ctl.sh')
