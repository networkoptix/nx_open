'Support for installing servers on physical hosts, controlled by yaml test config'

import os.path
import logging
import uuid
import jinja2
from .utils import is_list_inst
from .host import SshHostConfig, host_from_config
from .server_ctl import MEDIASERVER_DIR, PhysicalHostServerCtl
from .server_installation import MEDIASERVER_CONFIG_PATH, MEDIASERVER_CONFIG_PATH_INITIAL, ServerInstallation
from .server import Server

log = logging.getLogger(__name__)


TEST_UTILS_DIR = os.path.abspath(os.path.dirname(__file__))
SERVER_CTL_TEMPLATE_PATH = 'server_ctl.sh.jinja2'
SERVER_CTL_TARGET_PATH = 'server_ctl.sh'
SERVER_CONF_TEMPLATE_PATH = 'mediaserver.conf.jinja2'
SERVER_PORT_BASE = 1700



class PhysicalInstallationHostConfig(object):

    @classmethod
    def from_dict(cls, d):
        host = d.get('host')
        assert host, '"host" is required parameter for "hosts" elements'
        if host == 'localhost':
            location = None
        else:
            location = SshHostConfig.from_dict(d)
        root_dir = d.get('dir')
        assert root_dir, '"dir" is required parameter for "hosts" elements'
        limit = d.get('limit')
        return cls(
            name=d.get('name') or host,
            location=location,
            root_dir=root_dir,
            limit=limit,
            )

    def __init__(self, name, root_dir, location, limit=None):
        self.name = name
        self.location = location  # None for localhost or SshHostConfig, argument for host_from_config
        self.root_dir = root_dir
        self.limit = limit  # how much servers can be allocated here, None means unlimited

    def __repr__(self):
        return '<%r: %r at %r, %s>' % (self.name, self.root_dir, self.location,
                                       'limit %d' % self.limit if self.limit else 'unlimited')


class PhysicalInstallationCtl(object):

    def __init__(self, mediaserver_dist_path, customization_company_name, config_list):
        assert is_list_inst(config_list, PhysicalInstallationHostConfig), repr(config_list)
        self._installation_hosts = [
            PhysicalInstallationHost.from_config(config, customization_company_name, mediaserver_dist_path) for config in config_list]
        self._available_hosts = self._installation_hosts[:]

    def clean_all_installations(self):
        for host in self._installation_hosts:
            host.reset_installations()

    def allocate_server(self, config):
        while self._available_hosts:
            host = self._available_hosts[0]
            server = host.allocate_server(config)
            if server:
                # next time allocate on next one
                self._available_hosts = self._available_hosts[1:] + self._available_hosts[:1]
                return server
            else:
                del self._available_hosts[0]
        return None

    def release_all(self):
        log.info('Releasing all physical installations')
        for host in self._installation_hosts:
            host.release_all_servers()
        self._available_hosts = self._installation_hosts[:]


class PhysicalInstallationHost(object):

    @classmethod
    def from_config(cls, config, customization_company_name, mediaserver_dist_path):
        return cls(config.name, host_from_config(config.location), config.root_dir, config.limit,
                   customization_company_name, mediaserver_dist_path)

    def __init__(self, name, host, root_dir, limit, customization_company_name, mediaserver_dist_path):
        self._name = name
        self._host = host  # Host (LocalHost or RemoteSshHost)
        self._root_dir = host.expand_path(root_dir)
        self._limit = limit
        self._customization_company_name = customization_company_name
        self._mediaserver_dist_path = mediaserver_dist_path
        self._dist_unpacked = self._host.dir_exists(self._remote_dist_subdir)
        self._ensure_root_dir_exists()
        self._installations = list(self._read_installations())  # ServerInstallation list
        self._available_installations = self._installations[:]  # ServerInstallation list, set up but yet unallocated
        self._allocated_server_list = []
        self._timezone = host.get_timezone()
        self._ensure_servers_are_stopped()  # expected initial state, we may reset_installations after this, so pids will be lost

    def reset_installations(self):
        log.info('%s: removing directory: %s', self._name, self._root_dir)
        self._host.rm_tree(self._root_dir, ignore_errors=True)
        self._dist_unpacked = False
        self._installations = []
        self._available_installations = []

    def _ensure_root_dir_exists(self):
        self._host.mk_dir(self._root_dir)

    def allocate_server(self, config):
        if self._available_installations:
            installation = self._available_installations.pop(0)
            if config.leave_initial_cloud_host:
                self._reset_installation_dir(installation)
        else:
            installation = self._create_installation()
            if not installation:
                return None
        server_port = self._installation_server_port(self._installations.index(installation))
        rest_api_url = '%s://%s:%d/' % (config.http_schema, self._host.host, server_port)
        server_ctl = PhysicalHostServerCtl(self._host, installation.dir)
        server = Server(config.name, self._host, installation, server_ctl, rest_api_url,
                        config.rest_api_timeout_sec, internal_ip_port=server_port, timezone=self._timezone)
        self._allocated_server_list.append(server)
        return server

    def release_all_servers(self):
        for server in self._allocated_server_list:
            server.ensure_service_status(is_started=False)
        self._allocated_server_list = []
        self._available_installations = self._installations[:]

    def _read_installations(self):
        for dir in sorted(self._host.expand_glob(os.path.join(self._root_dir, 'server-*'))):
            yield ServerInstallation(self._host, dir)

    def _ensure_servers_are_stopped(self):
        for installation in self._installations:
            server_ctl = PhysicalHostServerCtl(self._host, installation.dir)
            if server_ctl.get_state():
                server_ctl.set_state(is_started=False)

    def _create_installation(self):
        if self._limit and len(self._installations) >= self._limit:
            return None
        idx = len(self._installations)
        dir = os.path.join(self._root_dir, 'server-%03d' % (idx + 1))
        self._prepare_installation_dir(dir, self._installation_server_port(idx))
        installation = ServerInstallation(self._host, dir)
        self._installations.append(installation)
        return installation

    def _installation_server_port(self, installation_idx):
        return SERVER_PORT_BASE + installation_idx + 1

    @property
    def _remote_dist_root(self):
        return os.path.join(self._root_dir, 'dist')

    @property
    def _unpacked_dist_dir(self):
        return os.path.join(self._remote_dist_root, 'unpacked')

    @property
    def _remote_dist_subdir(self):
        return os.path.join(self._unpacked_dist_dir, MEDIASERVER_DIR.format(
            customization_company_name=self._customization_company_name))

    def _reset_installation_dir(self, installation):
        self._host.rm_tree(installation.dir)
        server_port = self._installation_server_port(self._installations.index(installation))
        self._prepare_installation_dir(installation.dir, server_port)
        
    def _prepare_installation_dir(self, dir, server_port):
        if not self._dist_unpacked:
            remote_dist_path = os.path.join(self._remote_dist_root, os.path.basename(self._mediaserver_dist_path))
            self._host.mk_dir(self._remote_dist_root)
            self._host.put_file(self._mediaserver_dist_path, self._remote_dist_root)
            self._host.run_command(['dpkg', '--extract', remote_dist_path, self._unpacked_dist_dir])
            assert self._host.dir_exists(self._remote_dist_subdir), (
                'Distributive provided was build with another customization.'
                ' Distributive customization company name is %r, but expected is %r' %
                (os.path.basename(self._host.expand_glob(os.path.join(self._unpacked_dist_dir, 'opt', '*'))[0]),
                 self._customization_company_name))
            self._dist_unpacked = True
        self._host.mk_dir(dir)
        self._host.run_command(['cp', '-a'] +
                               self._host.expand_glob(os.path.join(self._remote_dist_subdir, '*'), for_shell=True) +
                               [dir + '/'])
        self._write_server_ctl(dir)
        self._write_server_conf(dir, server_port)

    def _write_server_ctl(self, dir):
        contents = self._expand_template(
            dir, SERVER_CTL_TEMPLATE_PATH,
            SERVER_DIR=dir,
            )
        server_ctl_path = os.path.join(dir, SERVER_CTL_TARGET_PATH)
        self._host.write_file(server_ctl_path, contents)
        self._host.run_command(['chmod', '+x', server_ctl_path])

    def _write_server_conf(self, dir, server_port):
        contents = self._expand_template(
            dir, SERVER_CONF_TEMPLATE_PATH,
            SERVER_DIR=dir,
            SERVER_PORT=server_port,
            SERVER_GUID=str(uuid.uuid4()),
            )
        self._host.write_file(os.path.join(dir, MEDIASERVER_CONFIG_PATH), contents)
        self._host.write_file(os.path.join(dir, MEDIASERVER_CONFIG_PATH_INITIAL), contents)
    
    def _expand_template(self, dir, src_path, **kw):
        with open(os.path.join(TEST_UTILS_DIR, src_path)) as f:
            template = jinja2.Template(f.read())
        return template.render(**kw)
